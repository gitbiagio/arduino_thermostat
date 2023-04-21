#include <Wire.h>

#include "DHT.h"

#define LCD_BL        3
#define DHT_DATA      9 
#define LCD_DATA      5
#define LCD_WR        6
#define LCD_CS	      7
#define releOutput    2
#define encoderPin1   11
#define encoderPin2   10
#define encoderButton 12

#define CS1    digitalWrite(LCD_CS, HIGH) 
#define CS0    digitalWrite(LCD_CS, LOW)
#define WR1    digitalWrite(LCD_WR, HIGH) 
#define WR0    digitalWrite(LCD_WR, LOW)
#define DATA1  digitalWrite(LCD_DATA, HIGH) 
#define DATA0  digitalWrite(LCD_DATA, LOW)


#define uchar   unsigned char 
#define uint   unsigned int 
#define  ComMode    0x52
#define  RCosc      0x30
#define  LCD_on     0x06
#define  LCD_off    0x04
#define  Sys_en     0x02
#define  CTRl_cmd   0x80
#define  Data_cmd   0xa0

DHT dht(DHT_DATA,DHT22);

int encoderState = 0; // relative position
int encoderValue = 0; // pulse counter

// LCD segment mapping
//                         0    1    2    3    4    5    6    7    8    9    A    b
const char lcd_map[] = {0x7D,0x60,0x3E,0x7A,0x63,0x5B,0x5F,0x70,0x7F,0x7B,0x77,0x4F,0x1D,0x0E,0x6E,0x1F,0x17,0x67,0x47,0x0D,0x46,0x75,0x37,0x06,0x0F,0x6D,0x02,0x00,};
const char lcd_a = 0x10; ///           aaaa
const char lcd_b = 0x20; ///          f    b
const char lcd_c = 0x40; ///          f    b
const char lcd_d = 0x08; ///           gggg
const char lcd_e = 0x04; ///          e    c
const char lcd_f = 0x01; ///          e    c
const char lcd_g = 0x02; ///           dddd
const char lcd_dp= 0x80; ///                  dp

void SendBit_1621(uchar sdata,uchar cnt)
{ 
	uchar i; 
	for(i=0;i<cnt;i++) 
	{ 
		WR0;
		if(sdata&0x80) DATA1; 
		else DATA0;
		WR1;
		sdata<<=1; 
	} 
}

void SendCmd_1621(uchar command)
{ 
	CS0; 
	SendBit_1621(0x80,4);
	SendBit_1621(command,8);
	CS1;
}

void Write_1621(uchar addr,uchar sdata)
{ 
	addr<<=2; 
	CS0; 
	SendBit_1621(0xa0,3);
	SendBit_1621(addr,6);
	SendBit_1621(sdata,8);
	CS1; 
} 

void HT1621_all_off(uchar num)
{
	int i; 
	for(i=0;i<num;i++) { Write_1621(i*2,0x00); }
}

void HT1621_all_on(uchar num)
{
  int i; 
  for(i=0;i<num;i++) { Write_1621(i*2,0xFF); }
}

void Init_1621(void)
{
	SendCmd_1621(Sys_en);
	SendCmd_1621(RCosc);    
	SendCmd_1621(ComMode);  
	SendCmd_1621(LCD_on);
}    

void setup() {
  Serial.begin(115200);
  Serial.print(millis());
  Serial.println(F(" Setup pins"));
  pinMode(encoderPin1, INPUT_PULLUP);
  pinMode(encoderPin2, INPUT_PULLUP);
  pinMode(encoderButton, INPUT_PULLUP);
  pinMode(releOutput, OUTPUT);
  pinMode(LCD_CS, OUTPUT);   // 
  pinMode(LCD_WR, OUTPUT);   // 
  pinMode(LCD_DATA, OUTPUT); // 
  pinMode(LCD_BL, OUTPUT); // 
  pinMode(13, OUTPUT); // 

  digitalWrite (releOutput, LOW);
  digitalWrite(LCD_BL, HIGH);
  CS1;
  DATA1;
  WR1;
  dht.begin();

  Init_1621();
  Write_1621(10,0);
  Write_1621(8,0);
  Write_1621(6,0);
  Write_1621(4,0);
  Write_1621(2,0);
  Write_1621(0,0);
  Serial.print(millis());
  Serial.println(F(" Setup done"));
}

int millis_prev = 0;
int timer_update = 0;
int timer_bl = 0;
int timer_init = 0;
int timer_encoder = 0;
int timer_backlight = 0;
int timer_check_sensor = 0;
bool backlight_status = false;
int backlight_value = 254;
int temp_now = 0;
int temp_set = 180;
unsigned char display_blink = 0;

#define CHECK_TIMER(t) if( t ) t--; else 

void loop() {
  int millis_now = millis();

  leggiEncoder();
  if( encoderValue ) {
    if( timer_encoder==0 ) encoderValue = 0;
    timer_encoder = 5000;
    timer_backlight = 5000;
    temp_set += encoderValue;
    encoderValue = 0;
  }
  
  if( millis_now!=millis_prev ) {
    millis_prev = millis_now;
    CHECK_TIMER( timer_encoder );
    CHECK_TIMER( timer_backlight );
    
    CHECK_TIMER( timer_bl ) {
        timer_bl = 10;
        if( timer_backlight==0 ) {
          if( backlight_value<254 ) backlight_value++;
        } else {
          if( backlight_value>192 ) backlight_value--;
        }
        analogWrite(LCD_BL, backlight_value);
    }

    CHECK_TIMER( timer_init ) {
        timer_init = 1000;
        Init_1621();
    }

    CHECK_TIMER( timer_update ) {
        CHECK_TIMER(timer_check_sensor);
        timer_update = 100;
        display_blink++;
        if( dht.read() ) {
          temp_now = dht.readTemperature()*10;
          timer_check_sensor = 10*60; // un minuto
        }
        if( timer_encoder ) {
          if( display_blink&0x03 ) {
            displayTemp(temp_set, 0);
          } else {
            displayNoTemp();
          }
        } else {
          if( timer_check_sensor ) {
            displayTemp(temp_now, 0);
            if( temp_now>temp_set+3 ) digitalWrite(releOutput, LOW);
            if( temp_now<temp_set-3 ) digitalWrite(releOutput, HIGH);
          } else {
            displayNATemp();
            digitalWrite(releOutput, LOW);
          }
          if( digitalRead(encoderButton)==LOW ) {
            timer_backlight = 5000;
          }
        }
     }
  }
}
/*
void displayTime() {
  int v;
  DateTime now = rtc.now();

  Write_1621(10,0);
  v = now.hour();
  if( v>23 ) v=23;
  if( v<0 ) v=0;
  Write_1621(8,lcd_map[v/10]);
  Write_1621(6,lcd_map[v%10]);
  v = now.second();
  Write_1621(4,(v&1)?lcd_g:0);
  v = now.minute();
  if( v>23 ) v=23;
  if( v<0 ) v=0;
  Write_1621(2,lcd_map[v/10]);
  Write_1621(0,lcd_map[v%10]);
}
*/
void displayClear() {
    Write_1621(10,0);
    Write_1621(8,0);
    Write_1621(6,0);
    Write_1621(4,0);
    Write_1621(2,0);
    Write_1621(0,0);
}

void displayNoTemp() {
    Write_1621(10,0);
    Write_1621(8,0);
    Write_1621(6,0);
    Write_1621(4,0);
    Write_1621(2,0x33);
    Write_1621(0,0x1D);
}

void displayNATemp() {
    Write_1621(10,0);
    Write_1621(8,lcd_g);
    Write_1621(6,lcd_g);
    Write_1621(4,lcd_g|lcd_dp);
    Write_1621(2,0x33);
    Write_1621(0,0x1D);
}

void displayTemp(int t, unsigned char c) {
    if( t>=0 ) {
    Write_1621(10,c);
    if( t>99 ) {
      if( t>999 ) t=999;
      Write_1621(8,lcd_map[t/100]);
    } else {
      Write_1621(8,0x00);
    }
    Write_1621(6,lcd_map[(t/10)%10]);
    Write_1621(4,lcd_map[t%10]|lcd_dp);
    Write_1621(2,0x33);
    Write_1621(0,0x1D);
  } else {
    t = -t;
    Write_1621(10,lcd_g|c);
    if( t>99 ) {
      if( t>999 ) t=999;
      Write_1621(8,lcd_map[t/100]);
    } else {
      Write_1621(8,0x00);
    }
    Write_1621(6,lcd_map[(t/10)%10]);
    Write_1621(4,lcd_map[t%10]|lcd_dp);
    Write_1621(2,0x33);
    Write_1621(0,0x1D);
  }
}

void leggiEncoder() {
  // pin are pull-up when open, so i have to negate all inputs to have 1 when pressed
  int pin1 = 1-digitalRead(encoderPin1);
  int pin2 = 1-digitalRead(encoderPin2);

  int pos = (pin1 + 2*pin2);
  // Pos 0 1 3 2 0 2 3 1 0
  // DIR . H H . . . H H .
  // VAL . . H H . H H . H
  switch( encoderState ) {
  case -1:
    if( pos==0 ) encoderState = 0;
    break;
  case 0:
    if( pos==1 ) encoderState = 1;
    if( pos==2 ) encoderState = 4;
    if( pos==3 ) encoderState = -1;
    break;
    
  case 1: // pos 0->1->3
    if( pos==0 ) encoderState = 0;
    if( pos==2 ) encoderState = -1;
    if( pos==3 ) encoderState = 2;
    break;
  case 2: // pos 1->3->2
    if( pos==0 ) encoderState = -1;
    if( pos==1 ) encoderState = 1;
    if( pos==2 ) encoderState = 3;
    break;
  case 3: // pos 3->2->0
    if( pos==0 ) encoderState = 0;
    if( pos==1 ) encoderState = -1;
    if( pos==3 ) encoderState = 2;
    if( pos==0 ) encoderValue++;
    break;
    
  case 4: // pos 0->2->3
    if( pos==0 ) encoderState = 0;
    if( pos==1 ) encoderState = -1;
    if( pos==3 ) encoderState = 5;
    break;
  case 5: // pos 2->3->1
    if( pos==0 ) encoderState = -1;
    if( pos==1 ) encoderState = 6;
    if( pos==2 ) encoderState = 4;
    break;
  case 6: // pos 3->1->0
    if( pos==0 ) encoderState = 0;
    if( pos==2 ) encoderState = -1;
    if( pos==3 ) encoderState = 5;
    if( pos==0 ) encoderValue--;
    break;
  }
}
