#include <avr/io.h>
#include <util/delay.h>
#include "hd44780.h"
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define PRESKALER 1
#define TRIAK PD5
#define PRZEKAZNIK PC5
#define MODUL_GSM PC4
#define MODUL_BT PC3
#define WYSWIETLACZ_LCD PC2
#define LED PA6
#define ADC_POT PC3
#define ADC_LM35 PC0
#define KLASKACZ PA2
#define PRZYCISK_ZATRZYMAJ (1<<PD2)
#define PRZYCISK_TRYBU (1<<PD3)
#define TRUE 1
#define FALSE 0
#define PIN_PRZYCISKOW PIND
#define WEJSCIA_PRZYCISKOW (PIN_PRZYCISKOW & (PRZYCISK_ZATRZYMAJ|PRZYCISK_TRYBU))
#define PROG_TRZYMANIA 2000
#define PROG_MRUGANIA 20
#define ROZMIAR_BUFORA 64

enum Tryb {
    FORCE_OFF = 0, PRE_GSM = 1, GSM = 2, PRE_BT = 4, BT = 5,
    PRE_POT = 6, POT = 7, PRE_STOP = 8, ON_MIN_SET = 9, ON_HOUR_SET = 10,
    OFF_MIN_SET = 11, OFF_HOUR_SET = 12, TIME_CONTROL = 13, FORCE_ON = 14,
    FORCE_ON_SET = 15, KLASK_CONTROL = 16, KLASK_CONTROL_SET = 17
};

enum Tryb eAktywnyTryb;
enum OutputRequest {OFF = 0, ON = 1, TOGGLE = 2, NO_REQUEST = 3};

unsigned char ucWejsciaPrzyciskowNowe = 0;
unsigned char ucWejsciaPrzyciskowStare = 0;
unsigned char ucWejsciaPrzyciskowXOR = 0;
unsigned int uiLicznikPozNeutralnej = 0;

unsigned char sekundy, minuty, godziny;
unsigned char sekundy_dziesiatki, sekundy_jednostki;
unsigned char minuty_dziesiatki, minuty_jednostki;
unsigned char godziny_dziesiatki, godziny_jednostki;

unsigned char ucKlaskFlag = 0;
unsigned char ucKlaskTemporaryMuteFlag = 0;

unsigned char ON_godzina = 21;
unsigned char ON_minuta = 31;
unsigned char OFF_godzina = 21;
unsigned char OFF_minuta = 35;

void ADC_Init(void);
uint16_t ADC_read(uint8_t channel);
void USART_Init(void);
void USART_Wyslij(char cTekst[]);
void PWM_inicjacja(void);
void Wyswietlanie(char cDane[]);
void Obsluga_Przyciskow(void);
void Przyciski_Init(void);
int Sprawdz_Tekst(char *cTekst, char *cWzorzec);
void ZapewnijMiejsceDlaKolejnejWiadomosci(void);
void WczytujDane(void);
void SprawdzDane(void);
void Wyjscia_Init(void);
void ZmianaWypelnienia(void);
void ObslugaWyswietlacza(void);
void Timer0_inicjacja(void);
void TWI_inicjacja(void);
unsigned char TWI_odczyt(unsigned char adres_rejestru);
void TWI_zapis(unsigned char adres_rejestru, unsigned char liczba);
void TimeTWIRead(void);
char TimeMatchCheck(int godzina_zal, int minuta_zal);
void ObslugaKlaskacza(void);
void OutputControl(int Request);

volatile int iFlagaDoWyslania;
char WzorWiadOdebr[] = "+CMTI:";
char WzorMetadanych[] = "+CMGR:";
char cOdbierzSMS[16] = "AT+CMGR=x";
char cUsunSMS[16] = "AT+CMGD=x";

char cLinijka1[17];
char cLinijka2[17];
char cTrescWiadomosci2[150] = "NO MESSAGE!";
char cTrescWiadomosci1[150] = "ALARM";
char cMetaDaneWiadomosci[65];
char cOdebranaKomenda[64];
char *cDane;
char cFlagaZapelnieniaPamieci;

// Bufor dla dynamicznego podglądu i przewijania odebranego SMS-a na LCD
char cOdebranySMS_Podglad[100] = "";
volatile unsigned int uiLicznikPodgladuSMS = 0;
int iOffsetPrzewijania = 0;

volatile unsigned char vucBuforCykliczny[ROZMIAR_BUFORA];
volatile int viIndeksZapisu;
volatile int viIndeksOdczytu;
int iLicznikDanych;

volatile unsigned int uiLicznikMrugania = 0;
volatile unsigned int uiICR_Prim;
volatile int iCzestotliwosc;
volatile double dWypelnienieProcent;
int n;
char cIndeksWiadomosciOdebr[4] = "";
char cStan;

enum Stan {
    OCZEKIWANIE_NA_POWIADOMIENIE_O_WIADOMOSCI = 0,
    OCZEKIWANIE_NA_METADANE = 1,
    OCZEKIWANIE_NA_TRESC = 2,
    OCZEKIWANIE_NA_ANALIZE = 3
};

int iIndeks = 0;

int main(void) {
    Przyciski_Init();
    TWI_inicjacja();
    ADC_Init();
    PWM_inicjacja();
    Timer0_inicjacja();

    iLicznikDanych = 0;
    cFlagaZapelnieniaPamieci = 0;
    Wyjscia_Init();

    lcd_init();
    LCD_DISPLAY(LCDDISPLAY);

    PORTC |= (1 << MODUL_GSM);
    sei();

    USART_Init();
    _delay_ms(5000);

    USART_Wyslij("ATE0");
    _delay_ms(1000);
    USART_Wyslij("AT+CMGF=1");
    _delay_ms(1000);
    USART_Wyslij("AT+CPMS=\"SM\",\"SM\",\"SM\"");
    _delay_ms(1000);
    USART_Wyslij("AT+CMGD=1,4"); // <--- Czyszczenie całej pamięci SMS przy starcie
    _delay_ms(500);

    viIndeksZapisu = 0;
    viIndeksOdczytu = 0;

    cStan = OCZEKIWANIE_NA_POWIADOMIENIE_O_WIADOMOSCI;
    cDane = cOdebranaKomenda;

    // Domyślny tryb pracy po uruchomieniu mikrokontrolera
    eAktywnyTryb = GSM;

    while(1) {
        TimeTWIRead();

        switch(eAktywnyTryb) {
            case GSM:
                // Sterowanie odbywa się automatycznie poprzez odebrane wiadomości SMS w SprawdzDane()
                break;
            case TIME_CONTROL:
                if (TimeMatchCheck(ON_godzina, ON_minuta) == TRUE) {
                    OutputControl(ON);
                }
                if (TimeMatchCheck(OFF_godzina, OFF_minuta) == TRUE) {
                    OutputControl(OFF);
                }
                break;
            case FORCE_ON:
                OutputControl(ON);
                break;
            case KLASK_CONTROL:
                ObslugaKlaskacza();
                break;
            case FORCE_OFF:
                OutputControl(OFF);
                break;
            default:
                break;
        }

        WczytujDane();
        SprawdzDane();
        Obsluga_Przyciskow();
    }
    return 0;
}

char TimeMatchCheck(int godzina_zal, int minuta_zal) {
    char TimeMatch = FALSE;
    int godzina_zal_jednostki, godzina_zal_dziesiatki;
    int minuta_zal_jednostki, minuta_zal_dziesiatki;

    godzina_zal_dziesiatki = godzina_zal / 10;
    godzina_zal_jednostki = godzina_zal % 10;
    minuta_zal_dziesiatki = minuta_zal / 10;
    minuta_zal_jednostki = minuta_zal % 10;

    if (godziny_dziesiatki == godzina_zal_dziesiatki && godziny_jednostki == godzina_zal_jednostki) {
        if (minuty_dziesiatki == minuta_zal_dziesiatki && minuty_jednostki == minuta_zal_jednostki) {
            TimeMatch = TRUE;
        }
    }
    return TimeMatch;
}

void TimeTWIRead(void) {
    sekundy = TWI_odczyt(0x00);
    sekundy_dziesiatki = (sekundy >> 4);
    sekundy_jednostki = (sekundy & 0x0F);

    minuty = TWI_odczyt(0x01);
    minuty_dziesiatki = (minuty >> 4);
    minuty_jednostki = (minuty & 0x0F);

    godziny = TWI_odczyt(0x02);
    godziny_dziesiatki = (godziny >> 4);
    godziny_jednostki = (godziny & 0x0F);
}

void SprawdzDane(void) {
    char cBuforDlaOdebranejLiczby[15];
    int iLicznikZnakow;

    if (cStan == OCZEKIWANIE_NA_ANALIZE) {
        // Kopiowanie odebranej wiadomości do podglądu LCD i wyzwolenie czasu podglądu
        snprintf(cOdebranySMS_Podglad, sizeof(cOdebranySMS_Podglad), "%s", cTrescWiadomosci2);
        iOffsetPrzewijania = 0;
        uiLicznikPodgladuSMS = 250; // Podgląd aktywny na ok. 5 sekund

        // Konwersja na małe litery
        for (int i = 0; cTrescWiadomosci2[i] != '\0'; i++) {
            cTrescWiadomosci2[i] = tolower((unsigned char)cTrescWiadomosci2[i]);
        }

        if (Sprawdz_Tekst(cTrescWiadomosci2, "on")) {
            OutputControl(ON);
            dWypelnienieProcent = 100;
            ZmianaWypelnienia();
        }
        else if (Sprawdz_Tekst(cTrescWiadomosci2, "off")) {
            OutputControl(OFF);
            dWypelnienieProcent = 0;
            ZmianaWypelnienia();
        }
        else if (Sprawdz_Tekst(cTrescWiadomosci2, "w ")) {
            for (iLicznikZnakow = 2; cTrescWiadomosci2[iLicznikZnakow] != '\0'; iLicznikZnakow++) {
                cBuforDlaOdebranejLiczby[iLicznikZnakow - 2] = cTrescWiadomosci2[iLicznikZnakow];
            }
            cBuforDlaOdebranejLiczby[iLicznikZnakow - 2] = '\0';
            dWypelnienieProcent = atof(cBuforDlaOdebranejLiczby);
            ZmianaWypelnienia();
        }

        ZapewnijMiejsceDlaKolejnejWiadomosci();
        cDane = cOdebranaKomenda;
        cStan = OCZEKIWANIE_NA_POWIADOMIENIE_O_WIADOMOSCI;
    }
}

void Wyswietlanie(char cDane[]) {
    int i = 0;
    cLinijka1[0] = '\0';
    cLinijka2[0] = '\0';
    for (i = 0; cDane[i] != '\0'; i++) {
        if (i <= 15) {
            cLinijka1[i] = cDane[i];
            cLinijka1[i + 1] = '\0';
        } else {
            cLinijka2[i - 16] = cDane[i];
            cLinijka2[i - 15] = '\0';
            if (i >= 31) break;
        }
    }
    LCD_CLEAR;
    LCD_LOCATE(0, 0);
    lcd_puts(cLinijka1);
    LCD_LOCATE(0, 1);
    lcd_puts(cLinijka2);
}

void Obsluga_Przyciskow(void) {
    ucWejsciaPrzyciskowNowe = WEJSCIA_PRZYCISKOW;
    ucWejsciaPrzyciskowXOR = (ucWejsciaPrzyciskowNowe ^ ucWejsciaPrzyciskowStare);

    if (ucWejsciaPrzyciskowXOR & PRZYCISK_ZATRZYMAJ) {
        eAktywnyTryb = FORCE_OFF;
        uiLicznikPozNeutralnej = 0;
    }
    else if (ucWejsciaPrzyciskowXOR & PRZYCISK_TRYBU) {
        switch(eAktywnyTryb) {
            case FORCE_OFF:         eAktywnyTryb = FORCE_ON_SET; break;
            case FORCE_ON_SET:      eAktywnyTryb = KLASK_CONTROL_SET; break;
            case KLASK_CONTROL_SET: eAktywnyTryb = GSM; break;
            case GSM:               eAktywnyTryb = ON_HOUR_SET; break;
            case ON_HOUR_SET:
                ON_godzina++;
                if (ON_godzina > 23) ON_godzina = 0;
                break;
            case ON_MIN_SET:
                ON_minuta++;
                if (ON_minuta > 59) ON_minuta = 0;
                break;
            case OFF_HOUR_SET:
                OFF_godzina++;
                if (OFF_godzina > 23) OFF_godzina = 0;
                break;
            case OFF_MIN_SET:
                OFF_minuta++;
                if (OFF_minuta > 59) OFF_minuta = 0;
                break;
            default: break;
        }
        uiLicznikPozNeutralnej = 0;
    }
    else if (ucWejsciaPrzyciskowXOR == 0) {
        if (uiLicznikPozNeutralnej >= PROG_TRZYMANIA) {
            uiLicznikPozNeutralnej = 0;
            uiLicznikMrugania = 0;
            switch(eAktywnyTryb) {
                case FORCE_ON_SET:      eAktywnyTryb = FORCE_ON; break;
                case ON_HOUR_SET:       eAktywnyTryb = ON_MIN_SET; break;
                case ON_MIN_SET:        eAktywnyTryb = OFF_HOUR_SET; break;
                case OFF_HOUR_SET:      eAktywnyTryb = OFF_MIN_SET; break;
                case OFF_MIN_SET:       eAktywnyTryb = TIME_CONTROL; break;
                case KLASK_CONTROL_SET: eAktywnyTryb = KLASK_CONTROL; break;
                default: break;
            }
        }
        uiLicznikPozNeutralnej++;
    }
    ucWejsciaPrzyciskowStare = ucWejsciaPrzyciskowNowe;
}

void Przyciski_Init(void) {
    DDRD &= ~((PRZYCISK_ZATRZYMAJ) | (PRZYCISK_TRYBU));
    PORTD |= (PRZYCISK_ZATRZYMAJ | PRZYCISK_TRYBU);
    ucWejsciaPrzyciskowStare = WEJSCIA_PRZYCISKOW;
}

void USART_Init(void) {
    viIndeksZapisu = 0;
    viIndeksOdczytu = 0;
    UBRRH = 0;
    UBRRL = 51; // 9600 bps dla F_CPU = 8 MHz
    UCSRB = (1 << RXEN) | (1 << TXEN) | (1 << RXCIE);
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
}

void USART_Wyslij(char cTekst[]) {
    for (unsigned char i = 0; cTekst[i] != '\0'; i++) {
        while (!(UCSRA & (1 << UDRE)));
        UDR = cTekst[i];
    }
    while (!(UCSRA & (1 << UDRE)));
    UDR = 13; // CR
}

ISR(USART_RXC_vect) {
    if (viIndeksZapisu != (viIndeksOdczytu - 1)) {
        vucBuforCykliczny[viIndeksZapisu] = UDR;
        viIndeksZapisu++;
        if (viIndeksZapisu >= ROZMIAR_BUFORA) {
            viIndeksZapisu = 0;
        }
    }
}

ISR(TIMER0_OVF_vect) {
    ObslugaWyswietlacza();
}

int Sprawdz_Tekst(char *cTekst, char *cWzorzec) {
    for (int i = 0; cWzorzec[i] != '\0'; i++) {
        if (cWzorzec[i] != cTekst[i]) {
            return 0;
        }
    }
    return 1;
}

void ZapewnijMiejsceDlaKolejnejWiadomosci(void) {
    if (iIndeks >= 20) {
        cFlagaZapelnieniaPamieci = 1;
    }
    if (cFlagaZapelnieniaPamieci == 1) {
        sprintf(cUsunSMS, "AT+CMGD=%d", iIndeks);
        USART_Wyslij(cUsunSMS);

    }
}

void WczytujDane(void) {
    if (viIndeksOdczytu != viIndeksZapisu) {
        unsigned char c = vucBuforCykliczny[viIndeksOdczytu];

        if (c != 10) { // Ignoruj LF
            if (c != 13) { // Pobieraj do momentu napotkania CR
                if (iLicznikDanych < 149) {
                    cDane[iLicznikDanych] = c;
                    iLicznikDanych++;
                }
            } else {
                cDane[iLicznikDanych] = '\0';
                iLicznikDanych = 0;

                switch(cStan) {
                    case OCZEKIWANIE_NA_POWIADOMIENIE_O_WIADOMOSCI:
                        if (Sprawdz_Tekst(cDane, WzorWiadOdebr)) {
                            char *pPrzecinek = strchr(cDane, ',');
                            if (pPrzecinek != NULL) {
                                iIndeks = atoi(pPrzecinek + 1);
                                sprintf(cOdbierzSMS, "AT+CMGR=%d", iIndeks);
                                USART_Wyslij(cOdbierzSMS);
                                cStan = OCZEKIWANIE_NA_METADANE;
                                cDane = cMetaDaneWiadomosci;
                            }
                        }
                        break;

                    case OCZEKIWANIE_NA_METADANE:
                        if (Sprawdz_Tekst(cMetaDaneWiadomosci, "+CMGR")) {
                            cStan = OCZEKIWANIE_NA_TRESC;
                            cDane = cTrescWiadomosci2;
                        }
                        break;

                    case OCZEKIWANIE_NA_TRESC:
                        cStan = OCZEKIWANIE_NA_ANALIZE;
                        break;

                    default:
                        break;
                }
            }
        }

        viIndeksOdczytu++;
        if (viIndeksOdczytu >= ROZMIAR_BUFORA) {
            viIndeksOdczytu = 0;
        }
    }
}

void PWM_inicjacja(void) {
    uiICR_Prim = 7999;
    iCzestotliwosc = F_CPU / PRESKALER / (uiICR_Prim + 1);
    dWypelnienieProcent = 0;
    DDRD |= (1 << TRIAK);
    ICR1 = uiICR_Prim;
    OCR1A = uiICR_Prim * dWypelnienieProcent / 100;
    TCCR1A = (1 << WGM11) | (1 << COM1A1);
    TCCR1B = (1 << WGM12) | (1 << WGM13) | (1 << CS10);
}

void Timer0_inicjacja(void) {
    TCCR0 |= (1 << CS00) | (1 << CS02); // Preskaler 1024
    TIMSK |= (1 << TOIE0);
}

void ZmianaWypelnienia(void) {
    if (dWypelnienieProcent > 100) dWypelnienieProcent = 100;
    if (dWypelnienieProcent <= 0) {
        dWypelnienieProcent = 0;
        TCCR1A &= ~(1 << COM1A1);
    } else {
        OCR1A = uiICR_Prim * dWypelnienieProcent / 100;
        TCCR1A |= (1 << COM1A1);
    }
}

void ADC_Init(void) {
    DDRC &= ~((1 << ADC_POT) | (1 << ADC_LM35));
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Preskaler 128
}

uint16_t ADC_read(uint8_t channel) {
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

void Wyjscia_Init(void) {
    MCUCSR |= (1 << JTD);
    MCUCSR |= (1 << JTD);
    DDRA |= (1 << LED);
    PORTA &= ~(1 << LED);
    DDRC |= (1 << PRZEKAZNIK) | (1 << MODUL_GSM) | (1 << MODUL_BT) | (1 << WYSWIETLACZ_LCD);
    PORTC &= ~(1 << PRZEKAZNIK);
}

void ObslugaWyswietlacza(void) {
    // --- PIERWSZA LINIA LCD (PREZENTACJA TRYBU) ---
    switch(eAktywnyTryb) {
        case GSM:
            sprintf(cTrescWiadomosci1, "GSM CONTROL     ");
            break;

        case TIME_CONTROL:
            sprintf(cTrescWiadomosci1, "ON%02d:%02d OFF%02d:%02d", ON_godzina, ON_minuta, OFF_godzina, OFF_minuta);
            break;

        case FORCE_OFF:
            sprintf(cTrescWiadomosci1, "FORCE_OFF       ");
            break;

        case FORCE_ON:
        case FORCE_ON_SET:
            sprintf(cTrescWiadomosci1, "FORCE_ON        ");
            break;

        case KLASK_CONTROL:
        case KLASK_CONTROL_SET:
            sprintf(cTrescWiadomosci1, "KLASK_CONTROL   ");
            break;

        default:
            sprintf(cTrescWiadomosci1, "ON%02d:%02d OFF%02d:%02d", ON_godzina, ON_minuta, OFF_godzina, OFF_minuta);
            break;
    }

    // Miganie wybranego segmentu podczas konfiguracji harmonogramu
    if (uiLicznikMrugania <= PROG_MRUGANIA) {
        switch(eAktywnyTryb) {
            case FORCE_ON_SET:
            case KLASK_CONTROL_SET:
                memset(cTrescWiadomosci1, ' ', 16);
                break;
            case ON_HOUR_SET:  cTrescWiadomosci1[2] = ' '; cTrescWiadomosci1[3] = ' '; break;
            case ON_MIN_SET:   cTrescWiadomosci1[5] = ' '; cTrescWiadomosci1[6] = ' '; break;
            case OFF_HOUR_SET: cTrescWiadomosci1[11] = ' '; cTrescWiadomosci1[12] = ' '; break;
            case OFF_MIN_SET:  cTrescWiadomosci1[14] = ' '; cTrescWiadomosci1[15] = ' '; break;
            default: break;
        }
    }

    LCD_LOCATE(0, 0);
    lcd_puts(cTrescWiadomosci1);

    // --- DRUGA LINIA LCD (ZEGAR RTC LUB PODGLĄD SMS) ---
    LCD_LOCATE(0, 1);
    if (uiLicznikPodgladuSMS > 0) {
        int iDlugoscSMS = strlen(cOdebranySMS_Podglad);

        if (iDlugoscSMS <= 16) {
            // Krótki SMS - stały napis wyrównany spacjami
            char cBuforKrotki[17];
            snprintf(cBuforKrotki, sizeof(cBuforKrotki), "%-16s", cOdebranySMS_Podglad);
            lcd_puts(cBuforKrotki);
        } else {
            // Długi SMS - płynne przewijanie napisu w lewo
            char cOknoTekstowe[17];
            snprintf(cOknoTekstowe, sizeof(cOknoTekstowe), "%-16s", cOdebranySMS_Podglad + iOffsetPrzewijania);
            lcd_puts(cOknoTekstowe);

            // Przesuwanie o 1 znak co 4 cykle odświeżania ekranu
            if (uiLicznikMrugania % 4 == 0) {
                iOffsetPrzewijania++;
                if (iOffsetPrzewijania > (iDlugoscSMS - 12)) {
                    iOffsetPrzewijania = 0;
                }
            }
        }
        uiLicznikPodgladuSMS--;
    } else {
        // Zegar RTC wyświetla się TYLKO gdy minie czas podglądu SMS
        char cBuforZegara[17];
        sprintf(cBuforZegara, "%d%d:%d%d:%d%d        ", godziny_dziesiatki, godziny_jednostki, minuty_dziesiatki, minuty_jednostki, sekundy_dziesiatki, sekundy_jednostki);
        lcd_puts(cBuforZegara);
    }

    uiLicznikMrugania++;
    if (uiLicznikMrugania >= 2 * PROG_MRUGANIA) {
        uiLicznikMrugania = 0;
    }
}

void TWI_inicjacja(void) {
    TWSR = 0x00;
    TWBR = 0x20; // 100 kHz dla F_CPU = 8 MHz
}

unsigned char TWI_odczyt(unsigned char adres_rejestru) {
    unsigned char odczyt;
    unsigned char scalak = 0xD0;

    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    TWDR = scalak;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    TWDR = adres_rejestru;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    TWDR = scalak | 0x01;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    odczyt = TWDR;
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
    return odczyt;
}

void TWI_zapis(unsigned char adres_rejestru, unsigned char liczba) {
    unsigned char scalak = 0xD0;

    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    TWDR = scalak;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    TWDR = adres_rejestru;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    TWDR = liczba;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
}

void ObslugaKlaskacza(void) {
    #define MEASUREMENT_RANGE 300
    #define SOUND_THRESHOLD 350
    static unsigned int uiMeasurementIndex = 0;
    uint16_t ADC_val;

    if (uiMeasurementIndex < MEASUREMENT_RANGE) {
        ADC_val = ADC_read(2);
        uiMeasurementIndex++;
        if ((ADC_val < SOUND_THRESHOLD) && (ucKlaskTemporaryMuteFlag == 0)) {
            ucKlaskFlag = 1;
        }
    } else {
        if (ucKlaskFlag == 1) {
            OutputControl(TOGGLE);
            ucKlaskFlag = 0;
            ucKlaskTemporaryMuteFlag = 1;
        } else {
            ucKlaskTemporaryMuteFlag = 0;
        }
        uiMeasurementIndex = 0;
    }
}

void OutputControl(int Request) {
    switch(Request) {
        case OFF:
            PORTC &= ~(1 << PRZEKAZNIK);
            PORTA &= ~(1 << LED);
            break;
        case ON:
            PORTC |= (1 << PRZEKAZNIK);
            PORTA |= (1 << LED);
            break;
        case TOGGLE:
            PORTC ^= (1 << PRZEKAZNIK);
            PORTA ^= (1 << LED);
            break;
        case NO_REQUEST:
            break;
    }
}
