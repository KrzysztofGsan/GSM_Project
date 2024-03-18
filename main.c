#include <avr/io.h>
#include <util/delay.h>
#include "hd44780.h"
#include <avr/interrupt.h>
#include <stdio.h>//bez tego jest ostrze¿enie przy atoi
#include <stdlib.h>// bez tego jest ostrze¿enie przy sprintf
//#define F_CPU 8000000UL// to jest zdefiniowane w rodowisku EClipse
#define PRESKALER 1
#define TRIAK PD5
#define PRZEKAZNIK PC5
#define MODUL_GSM PC4
#define MODUL_BT PC3
#define WYSWIETLACZ_LCD PC2
#define LED PA6
#define ADC_POT PC3//PC5
#define ADC_LM35 PC0//PC4
#define KLASKACZ PA2
#define PRZYCISK_ZATRZYMAJ (1<<PD2)
#define PRZYCISK_TRYBU (1<<PD3)
#define TRUE 1
#define FALSE 0

#define PIN_PRZYCISKOW PIND
#define WEJSCIA_PRZYCISKOW (PIN_PRZYCISKOW & (PRZYCISK_ZATRZYMAJ|PRZYCISK_TRYBU))
#define PROG_TRZYMANIA 2000
#define PROG_MRUGANIA 20
#define ROZMIAR_BUFORA 32//32
enum Tryb {STOP = 0, PRE_GSM=1, GSM=2, PRE_BT=4, BT =5, PRE_POT= 6, POT=7, PRE_STOP=8, ON_MIN_SET=9, ON_HOUR_SET=10, OFF_MIN_SET=11, OFF_HOUR_SET=12, TIME_CONTROL=13, FORCE_ON=14 };
enum Tryb eAktywnyTryb = GSM;
char cTryb[17]= "GSM CONTROL";
unsigned char ucWejsciaPrzyciskowNowe=0;
unsigned char ucWejsciaPrzyciskowStare=0;
unsigned char ucWejsciaPrzyciskowXOR=0;
unsigned int uiLicznikPozNeutralnej=0;
unsigned char sekundy;
unsigned char minuty;
unsigned char godziny;
unsigned char sekundy_dziesiatki;
unsigned char sekundy_jednostki;
unsigned char minuty_dziesiatki;
unsigned char minuty_jednostki;
unsigned char godziny_dziesiatki;
unsigned char godziny_jednostki;

unsigned char ON_godzina=20;
unsigned char ON_minuta=30;
unsigned char OFF_godzina=21;
unsigned char OFF_minuta=00;

void ADC_Init(void);
void ADC_Pomiar(int Pin);
void USART_Init(void);
void PWM_inicjacja(void);
void Wyswietlanie(char cDane[]);
void Obsluga_Przyciskow(void);
void Przyciski_Init(void);
int Sprawdz_Tekst(char *cTekst, char *cWzorzec);
void USART_Wyslij(char cTekst[]);
void ZapewnijMiejsceDlaKolejnejWiadomosci(void);
void WczytujDane(void);
void SprawdzDane(void);
void Wyjscia_Init(void);
void ZmianaWypelnienia(void);
void ObslugaWyswietlacza(void);
void Timer0_inicjacja(void); //to jest potrzebne do mrugania wyswietlacza
//void Wystartowanie_Trybu(int eAktywnyTryb);
void TWI_inicjacja(void);
unsigned char TWI_odczyt(unsigned char adres_rejestru);
void TWI_zapis(unsigned char adres_rejestru, unsigned char liczba);
void TimeTWIRead(void);
char TimeMatchCheck(int godzina_zal, int minuta_zal);
void ADC_init(void);
uint16_t ADC_read(uint8_t channel);
//void WYSWIETLACZ_LCD_Off(void);

uint16_t ADC_val;
volatile int iFlagaDoWyslania;
char WzorWiadOdebr[] = "+CMTI: "; //char WzorWiadOdebr[] = "+CMTI: \"SM\",";
char WzorMetadanych[] = "+CMGR:";
char cOdbierzSMS[14]="AT+CMGR=x";
char cUsunSMS[14]="AT+CMGD=x";
//char cTemp[33];
char cLinijka1[17];//="0123456789ABCDEF";
char cLinijka2[17];//="123456789ABCDEF0";
//char cDebug[17];
char cTrescWiadomosci2[150]="NO MESSAGE!";
char cTrescWiadomosci1[150]="ALARM";
char cMetaDaneWiadomosci[65];
char cOdebranaKomenda[33];
char *cDane;
char cFlagaZapelnieniaPamieci;
unsigned char ucTWI_dane;

volatile unsigned char vucBuforCykliczny[ROZMIAR_BUFORA];//
volatile int viIndeksZapisu;
volatile int viIndeksOdczytu;
int iLicznikDanych;
char cBuforUART_Wysylanie[15];
unsigned int uiWynik_POT=3069;
unsigned int uiWynik_LM35=3069;
volatile unsigned int uiLicznikMrugania=0;
volatile unsigned int uiICR_Prim;
volatile int iCzestotliwosc;
volatile double dWypelnienieProcent;
int n;
char cIndeksWiadomosciOdebr[4]="";
char cStan;
enum Stan {OCZEKIWANIE_NA_POWIADOMIENIE_O_WIADOMOSCI=0, OCZEKIWANIE_NA_METADANE=1, OCZEKIWANIE_NA_TRESC=2, OCZEKIWANIE_NA_ANALIZE};
char iIndeks=0;

int main(void){
   Przyciski_Init();
   TWI_inicjacja();
//   TWI_zapis(0x00,0b00000000); //zeruj bit 'CH' - wlącz oscylator
//   TWI_zapis(0x00,0x00);//sekund
//   TWI_zapis(0x01,0x19);//minut
//   TWI_zapis(0x02,0x20);//godzina
   ADC_init();
   PWM_inicjacja();
   Timer0_inicjacja();
   iLicznikDanych=0;
   cFlagaZapelnieniaPamieci=0;


   Wyjscia_Init();

   lcd_init();
   //PORTC |= (1<<WYSWIETLACZ_LCD);
   LCD_DISPLAY(LCDDISPLAY);
   PORTC |= (1<<MODUL_GSM);
   sei();

  ADC_Init(); //odkomentowac
  USART_Init();
 _delay_ms(3000);
  USART_Wyslij("ATE0");
  _delay_ms(500);
  USART_Wyslij("AT+CMGF=1");
//  _delay_ms(500);
//  USART_Wyslij("AT+CPMS=\"ME_P\",\"ME_P\",\"ME_P\""); //USART_Wyslij("AT+CPMS=\"SM\""); /
//  _delay_ms(500);
//
//  cStan=OCZEKIWANIE_NA_POWIADOMIENIE_O_WIADOMOSCI;
//  cDane=cOdebranaKomenda; //domyslny bufor (dla stanu = 0)
//  Wyswietlanie("Oczekiwanie     na SMS..."); //odkomentuj
  eAktywnyTryb = FORCE_ON; //poczatkowy tryb
  while(1){
	  //_delay_ms(100);
	  ADC_val = ADC_read(2);

	  TimeTWIRead();

	  switch(eAktywnyTryb){
	  	  case TIME_CONTROL:
			  TimeTWIRead();
			  if ( TimeMatchCheck(ON_godzina,ON_minuta) == TRUE){
				  PORTC |= (1<<PRZEKAZNIK);
				  PORTA |=  (1<<LED);
			  }
			  if ( TimeMatchCheck(OFF_godzina,OFF_minuta) == TRUE){
				  PORTC &= ~(1<<PRZEKAZNIK);
				  PORTA &=  ~(1<<LED);
			  }
			  break;
	  	  case FORCE_ON:
			  PORTC |= (1<<PRZEKAZNIK);
			  PORTA |=  (1<<LED);
	  		  break;
	  	  default: //dla STOP i oraz podczas ustawiania godzin ON OFF wyłączymy wyjscie
	  		  PORTC &= ~(1<<PRZEKAZNIK);
	  		  PORTA &=  ~(1<<LED);
	  		  break;
	  }

	  //WczytujDane();
	  //SprawdzDane();
	  Obsluga_Przyciskow();
   }
   return 0;
}




char TimeMatchCheck(int godzina_zal, int minuta_zal){
	char TimeMatch; // 1: match, 0: no match
	int godzina_zal_jednostki, godzina_zal_dziesiatki, minuta_zal_jednostki, minuta_zal_dziesiatki;
	TimeMatch = FALSE;
	godzina_zal_dziesiatki = godzina_zal/10;
	godzina_zal_jednostki = (godzina_zal-10*godzina_zal_dziesiatki);
	minuta_zal_dziesiatki = minuta_zal/10;
	minuta_zal_jednostki = (minuta_zal - 10*minuta_zal_dziesiatki);

	if(godziny_dziesiatki==godzina_zal_dziesiatki && godziny_jednostki==godzina_zal_jednostki) {
		//godzina match

		if(minuty_dziesiatki==minuta_zal_dziesiatki && minuty_jednostki==minuta_zal_jednostki){
			//minuta match
			TimeMatch = TRUE;
		}
	}
	return TimeMatch;
}

void TimeTWIRead(void){
	  sekundy=TWI_odczyt(0x00);
	  sekundy_dziesiatki=(sekundy>>4);
	  sekundy_jednostki=(sekundy & 0b00001111);

	  minuty=TWI_odczyt(0x01);
	  minuty_dziesiatki=(minuty>>4);
	  minuty_jednostki=(minuty & 0b00001111);

	  godziny=TWI_odczyt(0x02);
	  godziny_dziesiatki=(godziny>>4);
	  godziny_jednostki=(godziny & 0b00001111);
}


void SprawdzDane(void){
	char cBuforDlaOdebranejLiczby[15];
	int iLicznikZnakow;
	if(cStan==OCZEKIWANIE_NA_ANALIZE){
		if ( Sprawdz_Tekst(cTrescWiadomosci2,"ON") || Sprawdz_Tekst(cTrescWiadomosci2,"On") ){
			dWypelnienieProcent=100;
			PORTA |=  (1<<LED);
			ZmianaWypelnienia();
		}
		else if( Sprawdz_Tekst(cTrescWiadomosci2,"OFF") || Sprawdz_Tekst(cTrescWiadomosci2,"Off") ){
			dWypelnienieProcent=0;
			PORTA &=  ~(1<<LED);
			ZmianaWypelnienia();
		}
		else{
			if ( Sprawdz_Tekst(cTrescWiadomosci2,"w ") || Sprawdz_Tekst(cTrescWiadomosci2,"W ")){
				for(iLicznikZnakow=2;cTrescWiadomosci2[iLicznikZnakow]!='\0';iLicznikZnakow++){
					cBuforDlaOdebranejLiczby[iLicznikZnakow-2]=cTrescWiadomosci2[iLicznikZnakow];
				}
				cBuforDlaOdebranejLiczby[iLicznikZnakow-2]='\0'; //nowosc
				dWypelnienieProcent=(atof(cBuforDlaOdebranejLiczby));
				ZmianaWypelnienia();
			}
		}

		ZapewnijMiejsceDlaKolejnejWiadomosci();
		cDane=cOdebranaKomenda;
		cStan=OCZEKIWANIE_NA_POWIADOMIENIE_O_WIADOMOSCI;
	}
}

void Wyswietlanie(char cDane[]){
	int i=0;

	cLinijka1[0]='\0';
	cLinijka2[0]='\0';

	for(i=0;cDane[i]!='\0';i++){
		if(i<=15){
			cLinijka1[i]=cDane[i];
			cLinijka1[i+1]='\0';
		}
		else{
			cLinijka2[i-16]=cDane[i];
			cLinijka2[i-15]='\0';

			if(i>=31){
				break;
			}
		}
	}
	LCD_CLEAR;
	LCD_LOCATE(0,0);
	lcd_puts(cLinijka1);

	LCD_LOCATE(0,1);
	lcd_puts(cLinijka2);
}

void Obsluga_Przyciskow(void){ //UWAGA! przyciski sa bistabilne! Nie widac czy są wciśniete czy nie wiec de facto mozemy reagować tylko na zbocze.
	ucWejsciaPrzyciskowNowe = WEJSCIA_PRZYCISKOW;
    ucWejsciaPrzyciskowXOR = (ucWejsciaPrzyciskowNowe^ucWejsciaPrzyciskowStare);
	if( ucWejsciaPrzyciskowXOR & PRZYCISK_ZATRZYMAJ){ //PRZYCISK_ZATRZYMAJ ma priorytet! func

		switch(eAktywnyTryb){
			case STOP:
				eAktywnyTryb = FORCE_ON;
				break;
			case FORCE_ON:
				eAktywnyTryb = STOP;
				break;
			default:
				eAktywnyTryb = STOP;
				break;
		}
		uiLicznikPozNeutralnej=0;
	}
	else if( ucWejsciaPrzyciskowXOR & PRZYCISK_TRYBU ){ //jesli sie zmienił stan

		switch(eAktywnyTryb){
			case TIME_CONTROL:
			case STOP:
				eAktywnyTryb = ON_HOUR_SET;  //po pierwszym nacisnieciu przycisku bedziemy ustawiac godziny zal/wyl poczynajac od godziny zalaczenia.
				break;
			case ON_HOUR_SET:
				ON_godzina=ON_godzina+1;
				if(ON_godzina>23){
					ON_godzina = 0;
				}
				break;
			case ON_MIN_SET:
				ON_minuta=ON_minuta+1;
					if(ON_minuta>59){
						ON_minuta = 0;
					}
				break;
			case OFF_HOUR_SET:
				OFF_godzina=OFF_godzina+1;
				if(OFF_godzina>23){
					OFF_godzina = 0;
				}
				break;
			case OFF_MIN_SET:
				OFF_minuta=OFF_minuta+1;
				if(OFF_minuta>59){
					OFF_minuta = 0;
				}
				break;
		}
		uiLicznikPozNeutralnej=0;
	}
	else if(ucWejsciaPrzyciskowXOR==0){ //jesli nie nacisnieto
		if(uiLicznikPozNeutralnej>=PROG_TRZYMANIA){
			uiLicznikPozNeutralnej=0;
			uiLicznikMrugania=0;
			switch(eAktywnyTryb){
				case ON_HOUR_SET:
					eAktywnyTryb=ON_MIN_SET;
					break;
				case ON_MIN_SET:
					eAktywnyTryb=OFF_HOUR_SET;
					break;
				case OFF_HOUR_SET:
					eAktywnyTryb=OFF_MIN_SET;
					break;
				case STOP: //podtrzymanie stopa gdy nic nie jest nacisniete
					eAktywnyTryb=STOP;
					break;
				case FORCE_ON:
					eAktywnyTryb = FORCE_ON; // podtrzymanie force on gdy nic nie jest nacisniete
					break;
				default:
					eAktywnyTryb=TIME_CONTROL;
					break;
			}
		}
		uiLicznikPozNeutralnej=uiLicznikPozNeutralnej+1;
	}


//	}
//	else{ //nie nacisnieto PRZYCISK_TRYBU
//		uiLicznikTrzymania=0;
//	}
	ucWejsciaPrzyciskowStare=ucWejsciaPrzyciskowNowe;
}

void Przyciski_Init(void){
	DDRD &= ~((PRZYCISK_ZATRZYMAJ)|(PRZYCISK_TRYBU));
	PORTD |= (PRZYCISK_ZATRZYMAJ |PRZYCISK_TRYBU); //podiciagniecie do +

	ucWejsciaPrzyciskowStare = WEJSCIA_PRZYCISKOW;
}

void USART_Init(void){
   viIndeksZapisu=0;
   viIndeksOdczytu=0;

   UBRRH = 0; // PREDKOSC 9600;  // trzeba zmienic jesli zmienimy f
   UBRRL = 51; //(dla 8Mhz 51, dla 1 -6, dla 16-103)

   UCSRB = (1<<RXEN)|(1<<TXEN) |(1<<RXCIE);//RXCIE0 przerwanie od odbioru
   UCSRC = (1<<URSEL) | (1<<UCSZ1) | (1<<UCSZ0);  // 1! bit stopu, danych8, bez parzystosci
}

void USART_Wyslij(char cTekst[]){ //WYSLIJ
   unsigned char iLicznikZnakow;

   for(iLicznikZnakow=0;cTekst[iLicznikZnakow]!='\0';iLicznikZnakow++){
      while ( !( UCSRA & (1<<UDRE)) );
   	  UDR = cTekst[iLicznikZnakow];
   }

   while ( !( UCSRA & (1<<UDRE)) );
   UDR = 13;
}

ISR(USART_RXC_vect){
   if(viIndeksZapisu != (viIndeksOdczytu-1) ){ //jesli jest miejsce (nie nadpiszemy nieodczytanych danych)
	   vucBuforCykliczny[viIndeksZapisu]=UDR;
	   viIndeksZapisu=viIndeksZapisu+1;
	   if(viIndeksZapisu>=ROZMIAR_BUFORA){
		   viIndeksZapisu=0;
	   }
   }
}

ISR(TIMER0_OVF_vect){
	ObslugaWyswietlacza();
}

int Sprawdz_Tekst(char *cTekst, char *cWzorzec){
	int iResult; // 0: NOK, 1: OK
	int i;

	iResult=1;

	for(i=0;cWzorzec[i]!='\0';i++){
		if( cWzorzec[i]!=cTekst[i] ){
			iResult=0;
			break;
		}
	}

	return iResult;
}

void ZapewnijMiejsceDlaKolejnejWiadomosci(void){
	if(iIndeks>=70){
		cFlagaZapelnieniaPamieci=1;
		iIndeks=20;
	}
	if(cFlagaZapelnieniaPamieci==1){
		sprintf(cUsunSMS,"AT+CMGD=%d",iIndeks+1);
		USART_Wyslij(cUsunSMS);
	}
}

void WczytujDane(void){
	if(viIndeksOdczytu!=viIndeksZapisu){
		if( (vucBuforCykliczny[viIndeksOdczytu]==10)  ){ //jesli LF
			//zrob nic
		}
		else if( (vucBuforCykliczny[viIndeksOdczytu]!=13) ){ //jesli zwykly znak
			cDane[iLicznikDanych]=vucBuforCykliczny[viIndeksOdczytu];
			iLicznikDanych++;
		}
		else{ //jesli CR
			cDane[iLicznikDanych]='\0';
			iLicznikDanych=0;
			//PORTA |=  (1<<LED);//usun
			switch(cStan){
		    	case OCZEKIWANIE_NA_POWIADOMIENIE_O_WIADOMOSCI:
		    		if ( Sprawdz_Tekst(cDane,WzorWiadOdebr) ){
		    			for(n=12;cDane[n]!='\0';n++){ // sprawdzimy czy nie jest to komenda o odebraniu SMS
		    				cIndeksWiadomosciOdebr[n-12]=cDane[n];
		    				cIndeksWiadomosciOdebr[n-11]='\0';
		    			}
		    			iIndeks=atoi(cIndeksWiadomosciOdebr);
		    			sprintf(cOdbierzSMS,"AT+CMGR=%d,1",iIndeks);
		    			USART_Wyslij(cOdbierzSMS);

		    			cStan=OCZEKIWANIE_NA_METADANE;
		    			cDane = cMetaDaneWiadomosci;
		    		}
		    		break;
		    	case OCZEKIWANIE_NA_METADANE:
		    		if ( Sprawdz_Tekst(cMetaDaneWiadomosci,"+CMGR") ){
		    			cStan = OCZEKIWANIE_NA_TRESC;
		    			cDane = cTrescWiadomosci2;
		    		}
		    		break;
		    	case OCZEKIWANIE_NA_TRESC:
		    		cStan = OCZEKIWANIE_NA_ANALIZE;
		    		ObslugaWyswietlacza();
		    		//cDane = cOdebranaKomenda; //to ustawimy po sprawdzeniu danych
		    		break;
				}
		}
		viIndeksOdczytu++;
		if(viIndeksOdczytu>=ROZMIAR_BUFORA){
			viIndeksOdczytu=0;
		}
	}
}

void PWM_inicjacja(void)
{
		uiICR_Prim = 7999; //
		iCzestotliwosc = F_CPU/PRESKALER/(uiICR_Prim+1); //(uiICR_Prim +1?)  //system daje niedokladnosc do 1 hz - zostaje tylko czesc calkowita

		dWypelnienieProcent=0; //startujemy z 0%

		DDRD |= (1<<TRIAK); // klucz

		ICR1=uiICR_Prim; //TOP
		OCR1A=uiICR_Prim*dWypelnienieProcent/100;

		TCCR1A	= (1<<WGM11) | (1<<COM1A1); // wyjscie OC1A ustaw przy bottom, przy compare wy³¹cz
	    TCCR1B = TCCR1B | ( (1<<WGM12)|(1<<WGM13)); ////TRYB 14 fast pwm - wgm11,12,13 DZIA£A! ALE SYMYLATOR JEST Z£Y!

		TCCR1B =TCCR1B | ((1<<CS10)/*|(1<<CS12)*/); //PRESKALER 1024 PIERWSZY!!!1

}

void Timer0_inicjacja(void)
{
	TCCR0 |= ((1<<CS00)|(1<<CS02)) ; //PRESKALER 1024
 	TIMSK |= (1<<TOIE0); //przerwanie na przepełnienie
}

void ZmianaWypelnienia(void){

	if(dWypelnienieProcent>100){
		dWypelnienieProcent=100;
    }
    if(dWypelnienieProcent<=0){//korekta
    	dWypelnienieProcent=0;
        TCCR1A = TCCR1A & (~(1<<COM1A1));
    }
    else{
	    OCR1A=uiICR_Prim*dWypelnienieProcent/100; // tu sie zawiesza dla w 100
	    TCCR1A = TCCR1A | (1<<COM1A1);

	//    sprintf(cInfoWyp,"Wyp. %3.1f%% ",dWypelnienieProcent);
	//    Wyswietlanie();
	//    iFlagaDoWyslania = 1;
    }
	//PORTA &= ~ (1<<LED);//usun
}

void ADC_Init(void)
{//ADC5
	DDRC &= ~(1<<ADC_POT); // ustawienie wejsc, PC5 - wejscie
	DDRC &= ~(1<<ADC_LM35); //PC4 wejscie
	PORTC = PORTC | ADC_POT | ADC_LM35;// podciagniecie
	ADMUX=(1<<REFS0);//|(1<<MUX0)|(1<<MUX2);//
	//ADCSRA |= (1<<ADEN);
}

void ADC_Pomiar(int Pin){

	int i;
	unsigned int uiWynik_Tymczasowy=0;
	unsigned int *uiWynik=&uiWynik_POT;

	if(Pin==ADC_POT){
		ADMUX=(1<<REFS0)|(1<<MUX0)|(1<<MUX1);// PC3
		uiWynik=&uiWynik_POT;
	}
	else if(Pin==ADC_LM35){
		ADMUX=(1<<REFS0); //|(MUXy=0;// PC0
		uiWynik=&uiWynik_LM35;
	}
	ADCSRA |= (1<<ADEN);

	uiWynik_Tymczasowy=0;
	for(i=0;i<=2;i++){
		ADCSRA|=(1<<ADSC);//pojedynczy pomiar
		while(ADCSRA & (1<<ADSC));
		uiWynik_Tymczasowy +=ADC;
	}
	ADCSRA &= ~(1<<ADEN);

	if( (uiWynik_Tymczasowy>*uiWynik) &&  ((uiWynik_Tymczasowy-*uiWynik)>=30) ){
		*uiWynik=uiWynik_Tymczasowy;
	}
	if( (uiWynik_Tymczasowy<*uiWynik) &&  ((*uiWynik-uiWynik_Tymczasowy)>=30) ){
		*uiWynik=uiWynik_Tymczasowy;
	}
	//dOpoznienieZaplonuProcent= (float) (3069-uiWynik_POT)*100/3069;
}

void Wyjscia_Init(void){
	//wylacz JTAG:
	MCUCSR |= (1<<JTD);
	MCUCSR |= (1<<JTD);

	DDRA|=(1<<LED);
	PORTA &= ~ (1<<LED);

	DDRC |= (1<<PRZEKAZNIK);
	DDRC |= (1<<MODUL_GSM);
	DDRC |= (1<<MODUL_BT);
	DDRC |= (1<<WYSWIETLACZ_LCD);
}

void ObslugaWyswietlacza(void){
	LCD_CLEAR;

	switch(eAktywnyTryb){
		case STOP:
			sprintf(cTrescWiadomosci1,"Stop Awaryjny!");
			break;
		case FORCE_ON:
			sprintf(cTrescWiadomosci1,"FORCE_ON");
			break;
		default:
			sprintf(cTrescWiadomosci1,"ON%02d:%02d OFF%02d:%02d",ON_godzina,ON_minuta,OFF_godzina,OFF_minuta);

			if(uiLicznikMrugania<=PROG_MRUGANIA){
				if(eAktywnyTryb==ON_HOUR_SET){
					cTrescWiadomosci1[2] = ' ';
					cTrescWiadomosci1[3] = ' ';
				}
				else if(eAktywnyTryb==ON_MIN_SET){
					cTrescWiadomosci1[5] = ' ';
					cTrescWiadomosci1[6] = ' ';
				}
				else if(eAktywnyTryb==OFF_HOUR_SET){
					cTrescWiadomosci1[11] = ' ';
					cTrescWiadomosci1[12] = ' ';
				}
				else if(eAktywnyTryb==OFF_MIN_SET){
					cTrescWiadomosci1[14] = ' ';
					cTrescWiadomosci1[15] = ' ';
				}
			}
			break;
	}

	LCD_LOCATE(0,0);
	lcd_puts(cTrescWiadomosci1);

	LCD_LOCATE(0,1);
	//sprintf(cTrescWiadomosci2,"%d%d:%d%d:%d%d",godziny_dziesiatki,godziny_jednostki,minuty_dziesiatki,minuty_jednostki,sekundy_dziesiatki,sekundy_jednostki);
	sprintf(cTrescWiadomosci2,"ADC: %d",ADC_val);
	lcd_puts(cTrescWiadomosci2);

	if(uiLicznikMrugania>=2*PROG_MRUGANIA){
		uiLicznikMrugania=0;
	}
	uiLicznikMrugania++;
}
/*
void Wystartowanie_Trybu(int eAktywnyTryb){
	switch(eAktywnyTryb){
		case GSM:
			sprintf(cTryb,"GSM CONTROL");
			break;
		case BT:
			sprintf(cTryb,"BT CONTROL");
			break;
		case POT:
			sprintf(cTryb,"POT CONTROL");
			break;
		case STOP:
			sprintf(cTryb,"STOP");
			break;
	}
}
*/
//TWI //I2C
void TWI_inicjacja(void)
{
  // Czestotliwosc TWI(i2c) = 100kHz (max 100kHz) -> atmega8 taktowana wewnetrznie 8MHz (RC=8MHz)
  // ------------------------------------------------
  /* TWSR = TWS7 TWS6 TWS5 TWS4 TWS3  -   TWPS1 TWPS0 */
  TWSR =0b00000000;    // Preskaler = 1  ->> TWPS1=0 TWPS0=0
  // ------------------------------------------------------
  /* TWBR = TWBR7 TWBR6 TWBR5 TWBR4 TWBR3 TWBR2 TWBR1 TWBR0 */
  TWBR =0b00100000;
  /* Rejestr odpowiedzialny za wybór współczynnika podziału dla generatora.
     Generator ten odpowiada za czestotliwosc która jest dzielona przez
     sygnał zegarowy SCL w trybie pracy Master.
     ->> TWBR musi byc wieksze od 10 dla stabilnej pracy TWI(i2c)
 Czyli ((8MHz/100kHz)-16)/2=32  => TWBR=32 lub TWBR=0x20 lub TWBR=0b00100000 */
}

unsigned char TWI_odczyt(unsigned char adres_rejestru){
 unsigned char odczyt;
 unsigned char scalak=0xD0;
 TWCR =(1 << TWINT) | (1 << TWSTA) | ( 1 << TWEN );
 while(!(TWCR & (1 << TWINT)));
 TWDR =scalak;
 TWCR =(1 << TWINT) | (1 << TWEN);
 while(!(TWCR & (1 << TWINT)));
 TWDR =adres_rejestru;
 TWCR =(1 << TWINT) | (1 << TWEN);
 while(!(TWCR & (1 << TWINT)));
 TWCR =(1 << TWINT) | (1 << TWSTA) | ( 1 << TWEN );
 while(!(TWCR & (1 << TWINT)));
 TWDR =scalak | 0x01;
 TWCR =(1 << TWINT) | (1 << TWEN);
 while(!(TWCR & (1 << TWINT)));
 TWCR =(1 << TWINT) | (1 << TWEN);
 while(!(TWCR & (1 << TWINT)));
 odczyt =TWDR;
 TWCR =(1 << TWINT) | (1<<TWEN) | (1<<TWSTO);
 return(odczyt);
}

void TWI_zapis(unsigned char adres_rejestru, unsigned char liczba)
 {
  unsigned char scalak=0xD0;
  TWCR =(1 << TWINT) | (1 << TWSTA) | ( 1 << TWEN );
  while(!(TWCR & (1 << TWINT)));
  TWDR =scalak;
  TWCR =(1 << TWINT) | (1 << TWEN);
  while(!(TWCR & (1 << TWINT)));
  TWDR =adres_rejestru;
  TWCR =(1 << TWINT) | (1 << TWEN);
  while(!(TWCR & (1 << TWINT)));
  TWDR =liczba;
  TWCR =(1 << TWINT) | (1 << TWEN);
  while(!(TWCR & (1 << TWINT)));
  TWCR =(1 << TWINT) | (1<<TWEN) | (1<<TWSTO);
}

void ADC_init() {
    // Ustawienia ADC: ustawienia napięcia referencyjnego, preskalera i inicjalizacja ADC
    ADMUX = (1 << REFS0);  // Ustawienie napięcia referencyjnego na AVcc
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);  // Włączenie ADC i ustawienie preskalera na 128
}

uint16_t ADC_read(uint8_t channel) {
    // Wybór kanału ADC
    ADMUX &= 0xF0;          // Wyczyszczenie starszych bitów
    ADMUX |= channel & 0x0F; // Ustawienie kanału ADC

    // Rozpoczęcie konwersji
    ADCSRA |= (1 << ADSC);

    // Oczekiwanie na zakończenie konwersji
    while (ADCSRA & (1 << ADSC));

    while ( !( UCSRA & (1<<UDRE)) );
    UDR = ADCH;
    while ( !( UCSRA & (1<<UDRE)) );
    UDR = ADCL;
    while ( !( UCSRA & (1<<UDRE)) );
    UDR = 13;



    // Zwrócenie wyniku konwersji
    return ADC;
}

/*
void WYSWIETLACZ_LCD_Off(void){
	PORTC &= ~(1<<WYSWIETLACZ_LCD);

	PORTB |=  _BV(PB0);
	PORTB |=  _BV(PB1);
	PORTB |=  _BV(PB4);
    PORTB |=  _BV(PB5);
    PORTB |=  _BV(PB6);
	PORTB |=  _BV(PB7);
	PORTA |=  _BV(PA7);
}
*/
