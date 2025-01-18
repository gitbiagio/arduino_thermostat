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

#define DELAY_RELE_ON    5    // ritardo di accensione rele [s]
#define DELAY_RELE_OFF   5    // ritardo di spegnimento rele [s]
#define DELAY_FALLBACK   3600*3
#define SHORT_PRESS      2
#define LONG_PRESS      20

int millis_prev = 0;
int timer_backlight = 5;
int timer_update = 0;
int timer_debounce = 0;
int timer_read = 0;
int timer_fallback = 0;
int temp_now = 180;
int input_temp = 180;
int target_temp = 180;
int default_temp = 180;
int button_up = 0;
int button_down = 0;
int delay_rele_on = DELAY_RELE_ON;
int delay_rele_off = DELAY_RELE_OFF;
int system_on = 0;
int target_locked = 0;

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

void incTargetTemp() {
  if( target_temp==input_temp && input_temp<600 ) input_temp += 5;
  target_temp = input_temp;
  timer_fallback = DELAY_FALLBACK;
}

void decTargetTemp() {
  if( target_temp==input_temp && target_temp>0 ) input_temp -= 5;
  target_temp = input_temp;
  timer_fallback = DELAY_FALLBACK;
}

void resetTargetTemp() {
  target_temp = default_temp;
}

int getTargetTemp() {
  return target_temp;
}

int getDefaultTemp() {
  return default_temp;
}

void lock() {
  target_locked = 1;
  timer_fallback = DELAY_FALLBACK;
}

void unlock() {
  target_locked = 0;
  timer_fallback = DELAY_FALLBACK;
}

void on() {
  system_on = 1;
  timer_fallback = DELAY_FALLBACK;
}

void off() {
  system_on = 0;
  timer_fallback = DELAY_FALLBACK;
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
  return;
}

void displayRefresh() {
  int temp_set = getTargetTemp();
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  if( system_on ) {
    if(temp_set>=100) display.drawBitmap(8,0,THINFONT24(temp_set/100),WHITE);
    display.drawBitmap(18,0,THINFONT24((temp_set%100)/10),WHITE);
    display.drawBitmap(28,11,THINFONT12(temp_set%10),WHITE);
    if( target_locked ) {
      display.fillRect(0,26,40,2,INVERSE);
    }
  }

  if(temp_now>=100) display.drawBitmap(55,0,THINFONT48(temp_now/100),WHITE);
  display.drawBitmap(75,0,THINFONT48((temp_now%100)/10),WHITE);
  display.drawBitmap(95,22,THINFONT24(temp_now%10),WHITE);

  display.display();
}

void checkButtons() {
    if( digitalRead(BUTTON_UP)==LOW ) {
      button_up++;
      if( button_up==LONG_PRESS && !button_down ) {
        lock();
      }
    } else {
      if( button_up>=SHORT_PRESS && button_up<LONG_PRESS && button_down==0 ) {
        if( !system_on ) on();
        else if( target_locked ) unlock();
        else incTargetTemp();
      }
      button_up = 0;
    }
    if( digitalRead(BUTTON_DOWN)==LOW ) {
      button_down++;
      if( button_down==LONG_PRESS && !button_up ) {
        off();
      }
    } else {
      if( button_down>=SHORT_PRESS && button_down<LONG_PRESS && button_up==0 ) {
        if( !system_on ) on();
        else if( target_locked ) unlock();
        else decTargetTemp();
      }
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
      int temp_set = getTargetTemp();
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
      if( !target_locked && timer_fallback ) {
        timer_fallback--;
        if( timer_fallback==0 && getTargetTemp()>getDefaultTemp() ) {
          resetTargetTemp();
        }
      }
      readTemp();
      displayRefresh();
      if( system_on ) {
        if( temp_now>temp_set+3 ) {
          if( delay_rele_off ) {
            delay_rele_off--;
          } else {
            digitalWrite(releOutput, LOW);
          }
        } else {
          delay_rele_off = DELAY_RELE_OFF;
        }
        if( temp_now<temp_set-3 ) {
          if( delay_rele_on ) {
            delay_rele_on--;
          } else {
            digitalWrite(releOutput, HIGH);
          }
        } else {
          delay_rele_on = DELAY_RELE_ON;
        }
      } else {
        delay_rele_on = DELAY_RELE_ON;
        timer_fallback = DELAY_FALLBACK;
        digitalWrite(releOutput, LOW);
      }
    }

    CHECK_TIMER( timer_debounce ) {
      timer_debounce = 20;
      checkButtons();
    }
  }
}
