// rele
#define releOutput        4

// push buttons
#define BUTTON_UP          3
#define BUTTON_DOWN         2

// display OLED
#define USE_SH1106      0
#define OLED_ADDR       0x3C
#if USE_SH1106
  #include <Adafruit_SH1106.h>
  Adafruit_SH1106 display(128,64);
#else
  #include <Adafruit_SSD1306.h>
  Adafruit_SSD1306 display(128,64);
#endif
#include "thinfont/thinfont.h"

#define CHECK_TIMER(t) if( t ) t--; else 
int millis_prev = 0;
int timer_backlight = 5;
int timer_update = 0;
int timer_debounce = 0;
int timer_read = 0;
int temp_now = 180;
int temp_set = 180;
int button_up = 0;
int button_down = 0;
int histeresys_on = 5;
int histeresys_off = 5;

#define ntcValueCount 10
const int16_t  ntcTemp[]  = {   0,  50, 100, 150, 200, 250, 300, 400, 500, 600 };
const uint16_t ntcValue[] = { 388, 450, 512, 572, 629, 682, 729, 808, 867, 910 };

#define TEMPSAMPLES 10
int temp_array[TEMPSAMPLES];
int temp_index;

void initTemp() {
  int i;
  for(i=0; i<TEMPSAMPLES; i++) temp_array[i] = temp_now;
  temp_index = 0;
}

void readTemp() {
  int value = analogRead(A0);
  if( value<ntcValue[0] ) {
    // out of range (lower)
    temp_now = 999;
  } else if( value>=ntcValue[ntcValueCount-1] ) {
    // out of range (upper)
    temp_now = 999;
  } else {
  	// compute the average of last TEMPSAMPLES readings
    int i;
    for(i=1; i<ntcValueCount; i++) if( value<=ntcValue[i] ) {
      temp_array[temp_index] = map(value, ntcValue[i-1], ntcValue[i], ntcTemp[i-1], ntcTemp[i]);
      temp_index++; if( temp_index==TEMPSAMPLES ) temp_index = 0;
      temp_now = 0;
      for(i=0; i<TEMPSAMPLES; i++) temp_now += temp_array[i];
      temp_now /= TEMPSAMPLES;
      break;
    }
  }
  return value;
}

void displayRefresh() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  if(temp_set>=100) display.drawBitmap(10-2,0,THINFONT24(temp_set/100),WHITE);
  display.drawBitmap(20-2,0,THINFONT24((temp_set%100)/10),WHITE);
  display.drawBitmap(30-2,11,THINFONT12(temp_set%10),WHITE);

  if(temp_now>=100) display.drawBitmap(55,0,THINFONT48(temp_now/100),WHITE);
  display.drawBitmap(75,0,THINFONT48((temp_now%100)/10),WHITE);
  display.drawBitmap(95,22,THINFONT24(temp_now%10),WHITE);

  display.display();
}

void checkButtons() {
    if( digitalRead(BUTTON_UP)==LOW ) {
      button_up++;
    } else {
      button_up = 0;
    }
    if( digitalRead(BUTTON_DOWN)==LOW ) {
      button_down++;
    } else {
      button_down = 0;
    }

    if( button_up || button_down ) {
      if( timer_backlight==0 ) {
        display.ssd1306_command(SSD1306_SETCONTRAST); //0x81
        display.ssd1306_command(255);
        display.ssd1306_command(SSD1306_SETPRECHARGE); //0xD9
        display.ssd1306_command(0xF1);
        display.ssd1306_command(SSD1306_SETVCOMDETECT); //0xDB
        display.ssd1306_command(0x40);
      }
      timer_backlight = 15;
      displayRefresh();
    }
    if( button_up==2 && !button_down ) {
      if( temp_set<600 ) temp_set += 5;
    }
    if( button_down==2 && !button_up ) {
      if( temp_set>0 ) temp_set -= 5;
    }
}

void setup() {
  //Serial.begin(9600);
  pinMode(releOutput, OUTPUT);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  digitalWrite (releOutput, LOW);

#if USE_SH1106
  display.begin(SH1106_SWITCHCAPVCC, OLED_ADDR);
#else
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
#endif
  //display.clearDisplay();
  display.display();
  initTemp();
}

void loop() {
  int millis_now = millis();

  if( millis_now!=millis_prev ) {
    millis_prev = millis_now;
    
    CHECK_TIMER( timer_update ) {
      timer_update = 1000;
      if( timer_backlight ) {
        timer_backlight--;
        if( timer_backlight==0 ) {
          display.ssd1306_command(SSD1306_SETCONTRAST); //0x81
          display.ssd1306_command(255);
          display.ssd1306_command(SSD1306_SETPRECHARGE); //0xD9
          display.ssd1306_command(0);
          display.ssd1306_command(SSD1306_SETVCOMDETECT); //0xDB
          display.ssd1306_command(0);
        }
      }
      readTemp();
      displayRefresh();
      if( temp_now>temp_set+3 ) {
        if( histeresys_off ) {
          histeresys_off--;
        } else {
          digitalWrite(releOutput, LOW);
        }
      } else {
        histeresys_off = 5;
      }
      if( temp_now<temp_set-3 ) {
        if( histeresys_on ) {
          histeresys_on--;
        } else {
          digitalWrite(releOutput, HIGH);
        }
      } else {
        histeresys_on = 5;
      }
    }

    CHECK_TIMER( timer_debounce ) {
      timer_debounce = 20;
      checkButtons();
    }
  }
}
