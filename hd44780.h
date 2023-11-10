/*
Plik hd44780.h
*/

#ifndef LCD_HD44780
#define LCD_HD44780

/* RS */
#define SET_OUT_LCD_RS  DDRB  |=  _BV(PB0)
#define SET_LCD_RS      PORTB |=  _BV(PB0)
#define CLR_LCD_RS      PORTB &= ~_BV(PB0)

/* RW */
#define SET_OUT_LCD_RW  DDRB  |=  _BV(PB1)
#define SET_LCD_RW      PORTB |=  _BV(PB1)
#define CLR_LCD_RW      PORTB &= ~_BV(PB1)

/* E */
#define SET_OUT_LCD_E   DDRB  |=  _BV(PB4)
#define SET_LCD_E       PORTB |=  _BV(PB4)
#define CLR_LCD_E       PORTB &= ~_BV(PB4)

/* D4 */
#define SET_OUT_LCD_D4  DDRB  |=  _BV(PB5)
#define SET_IN_LCD_D4   DDRB  &= ~_BV(PB5)
#define SET_LCD_D4      PORTB |=  _BV(PB5)
#define CLR_LCD_D4      PORTB &= ~_BV(PB5)
#define IS_SET_LCD_D4   PINB  &   _BV(PB5)

/* D5 */
#define SET_OUT_LCD_D5  DDRB  |=  _BV(PB6)
#define SET_IN_LCD_D5   DDRB  &= ~_BV(PB6)
#define SET_LCD_D5      PORTB |=  _BV(PB6)
#define CLR_LCD_D5      PORTB &= ~_BV(PB6)
#define IS_SET_LCD_D5   PINB  &   _BV(PB6)

/* D6 */
#define SET_OUT_LCD_D6  DDRB  |=  _BV(PB7)
#define SET_IN_LCD_D6   DDRB  &= ~_BV(PB7)
#define SET_LCD_D6      PORTB |=  _BV(PB7)
#define CLR_LCD_D6      PORTB &= ~_BV(PB7)
#define IS_SET_LCD_D6   PINB  &   _BV(PB7)

/* D7 */
#define SET_OUT_LCD_D7  DDRA  |=  _BV(PA7)
#define SET_IN_LCD_D7   DDRA  &= ~_BV(PA7)
#define SET_LCD_D7      PORTA |=  _BV(PA7)
#define CLR_LCD_D7      PORTA &= ~_BV(PA7)
#define IS_SET_LCD_D7   PINA  &   _BV(PA7)


#define LCD_NOP asm volatile("nop\n\t""nop\n\t" "nop\n\t" "nop\n\t" ::);



#define LCDCOMMAND 0
#define LCDDATA    1

#define LCD_LOCATE(x,y)  WriteToLCD(0x80|((x)+((y)*0x40)), LCDCOMMAND)

#define LCD_CLEAR              WriteToLCD(0x01, LCDCOMMAND)
#define LCD_HOME               WriteToLCD(0x02, LCDCOMMAND)

/* IDS */

#define LCDINCREMENT           0x02
#define LCDDECREMENT           0x00
#define LCDDISPLAYSHIFT        0x01

#define LCD_ENTRY_MODE(IDS)    WriteToLCD(0x04|(IDS), LCDCOMMAND)

/* BCD */
#define LCDDISPLAY             0x04
#define LCDCURSOR              0x02
#define LCDBLINK               0x01

#define LCD_DISPLAY(DCB)       WriteToLCD(0x08|(DCB), LCDCOMMAND)

/* RL */
#define LCDLEFT                0x00
#define LCDRIGHT               0x04

#define LCD_SHIFT_DISPLAY(RL)  WriteToLCD(0x18|(RL), LCDCOMMAND)
#define LCD_SHIFT_CURSOR(RL)   WriteToLCD(0x10|(RL), LCDCOMMAND)

#define LCD_CGRAM_ADDRESS(A)   WriteToLCD(0x40|((A)&0x3f), LCDCOMMAND)
#define LCD_DDRAM_ADDRESS(A)   WriteToLCD(0x80|((A)&0x7f), LCDCOMMAND)

#define LCD_WRITE_DATA(D)      WriteToLCD((D),LCDDATA)


void lcd_init(void);
void WriteToLCD(unsigned char v,unsigned char rs);
unsigned char ReadAddressLCD(void);
void lcd_puts(char *str);

#endif
