// ST7789 library example
// BME280 and DTH11 testing
// Required PropFont and Adafruit_BME280
// (c) 2020 Pawel A. Hernik
// YouTube video: https://youtu.be/-F7EWPt0yIo

/*
 ST7789 240x240 IPS (without CS pin) connections (only 6 wires required):

 #01 GND -> GND
 #02 VCC -> VCC (3.3V only!)
 #03 SCL -> D13/PA5/SCK
 #04 SDA -> D11/PA7/MOSI
 #05 RES -> D9 /PA0 or any digital
 #06 DC  -> D10/PA1 or any digital
 #07 BLK -> NC
*/

// BME280 pinout from the left:
// SDA SCL GND VCC

// DHT11 pinout from the left:
// VCC DATA NC GND

#include <SPI.h>
#include <Adafruit_GFX.h>
#if (__STM32F1__) // bluepill
#define TFT_DC    PA1
#define TFT_RST   PA0
#define BUTTON    PB9
#define DHT11_PIN PA2
//#include <Arduino_ST7789_STM.h>
#else
#define TFT_DC    10
#define TFT_RST   9
#define BUTTON    3
#define DHT11_PIN 7
#include <Arduino_ST7789_Fast.h>
#endif

#define SCR_WD 240
#define SCR_HT 240
Arduino_ST7789 lcd = Arduino_ST7789(TFT_DC, TFT_RST);

#include "PropFont.h"
#include "bold13x20digtop_font.h"
#include "term9x14_font.h"

PropFont font;
// needed for PropFont library initialization, define your drawPixel and fillRect
void customPixel(int x, int y, int c) { lcd.drawPixel(x, y, c); }
void customRect(int x, int y, int w, int h, int c) { lcd.fillRect(x, y, w, h, c); } 

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

Adafruit_BME280 bme; // I2C
#define SEA_LEVEL_PRESSURE (1013.25)

// --------------------------------------------------------------------------
int stateOld = HIGH;
long btDebounce    = 30;
long btDoubleClick = 600;
long btLongClick   = 700;
long btLongerClick = 2000;
long btTime = 0, btTime2 = 0;
int clickCnt = 1;

// 0=idle, 1,2,3=click, -1,-2=longclick
int checkButton()
{
  int state = digitalRead(BUTTON);
  if( state == LOW && stateOld == HIGH ) { btTime = millis(); stateOld = state; return 0; } // button just pressed
  if( state == HIGH && stateOld == LOW ) { // button just released
    stateOld = state;
    if( millis()-btTime >= btDebounce && millis()-btTime < btLongClick ) { 
      if( millis()-btTime2<btDoubleClick ) clickCnt++; else clickCnt=1;
      btTime2 = millis();
      return clickCnt; 
    } 
  }
  if( state == LOW && millis()-btTime >= btLongerClick ) { stateOld = state; return -2; }
  if( state == LOW && millis()-btTime >= btLongClick ) { stateOld = state; return -1; }
  return 0;
}
//-----------------------------------------------------------------------------

#define DHT_OK         0
#define DHT_CHECKSUM  -1
#define DHT_TIMEOUT   -2

float tempDHT,humidDHT;

int readDHT11(int pin)
{
  int temp1,temp10;
  uint8_t bits[5];
  uint8_t bit = 7;
  uint8_t idx = 0;

  for (int i = 0; i < 5; i++) bits[i] = 0;

  // REQUEST SAMPLE
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(18);
  digitalWrite(pin, HIGH);
  delayMicroseconds(40);
  pinMode(pin, INPUT_PULLUP);

  // ACKNOWLEDGE or TIMEOUT
  unsigned int loopCnt = 10000;
  while(digitalRead(pin) == LOW) if(!loopCnt--) return DHT_TIMEOUT;

  loopCnt = 10000;
  while(digitalRead(pin) == HIGH) if(!loopCnt--) return DHT_TIMEOUT;

  // READ OUTPUT - 40 BITS => 5 BYTES or TIMEOUT
  for (int i = 0; i < 40; i++) {
    loopCnt = 10000;
    while(digitalRead(pin) == LOW) if(!loopCnt--) return DHT_TIMEOUT;

    unsigned long t = micros();
    loopCnt = 10000;
    while(digitalRead(pin) == HIGH) if(!loopCnt--) return DHT_TIMEOUT;

    if(micros() - t > 40) bits[idx] |= (1 << bit);
    if(bit == 0) {
      bit = 7;    // restart at MSB
      idx++;      // next byte!
    }
    else bit--;
  }

  humidDHT = bits[0];
  temp1  = bits[2];
  temp10 = bits[3];
  tempDHT = abs(temp1+temp10/10.0);

  if(bits[4] != bits[0]+bits[1]+bits[2]+bits[3]) return DHT_CHECKSUM;
  return DHT_OK;
}
// --------------------------------------------------------------------------

unsigned long ms = 0;

void setup() 
{
  Serial.begin(9600);
  pinMode(BUTTON, INPUT_PULLUP);
  Wire.begin();
  Wire.setClock(400000);  // faster
  lcd.init(SCR_WD, SCR_HT);
  font.init(customPixel, customRect, SCR_WD, SCR_HT); // custom drawPixel and fillRect function and screen width and height values
  font.setFont(Term9x14); 
  font.setScale(1);
  bool status = bme.begin(0x76);  
  if(!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    lcd.fillScreen(RED);
    font.setScale(2,2);
    font.setColor(YELLOW);
    font.printStr(ALIGN_CENTER,120-50,"Could not find");
    font.printStr(ALIGN_CENTER,120,"a valid BME280");
    while(1); 
  }
}

const uint16_t lnCol  = RGBto565(255,150,255);
const uint16_t ln2Col = RGBto565(180,180,180);
const uint16_t labCol = RGBto565(250,250,250);
const uint16_t v1Col  = RGBto565(100,250,100);
const uint16_t v2Col  = RGBto565(255,250,100);
const uint16_t v3Col  = RGBto565(120,255,255);
const uint16_t v4Col  = RGBto565(255,120,120);
const uint16_t v1aCol = RGBto565(150,150,255);
const uint16_t v2aCol = RGBto565(250,150,250);
int xe=0,mode=1,lastMode=-1;

void drawField(int x, int y, int w, int h, char *label, uint16_t col=lnCol)
{
  lcd.drawRect(x,y+7,w,h-7,col);
  font.setFont(Term9x14); 
  font.setScale(1);
  font.setColor(labCol,BLACK);
  int wl = font.strWidth(label);
  font.printStr(x+(w-wl)/2,y,label);
}

void showVal(float v, int x, int y, int w, char *unit, uint16_t col)
{
  font.setFont(Bold13x20);
  font.setSpacing(3);
  font.setScale(1,2);
  font.setDigitMinWd(font.charWidth('0'));
  font.setColor(col,BLACK);
  char txt[10];
  dtostrf(v,w,1,txt);
  xe = font.printStr(x,y,txt);
  font.printStr(xe+2,y,unit);
}

void constBME280()
{
  drawField(    0,    0,120-5,120-3," Temp ");
  drawField(120+5,    0,120-5,120-3," Humidity ");
  drawField(    0,120+2,120-5,120-3," Pressure ");
  drawField(120+5,120+2,120-5,120-3," Altitude ");
  font.setColor(v3Col,BLACK);
  font.printStr(48,240-27,"hPa");
}

void varBME280()
{
  showVal(bme.readTemperature(), 18,43, 4,"'$",v1Col);
  showVal(bme.readHumidity(), 30+120,43, 4,"%",v2Col);
  showVal(bme.readPressure()/100.0, 14,122+43, 6,NULL,v3Col);
  showVal(bme.readAltitude(SEA_LEVEL_PRESSURE), 21+120,122+43, 5,NULL,v4Col);
  font.setFont(Term9x14); 
  font.printStr(xe+2,120+44+12,"m");
}

void constBoth()
{
  drawField(    0,  0,120-5,80-2," Temp ");
  drawField(120+5,  0,120-5,80-2," Humidity ");
  drawField(    0, 81,120-5,80-2," Pressure ");
  drawField(120+5, 81,120-5,80-2," Altitude ");
  drawField(    0,162,120-5,80-2," TempDHT ",ln2Col);
  drawField(120+5,162,120-5,80-2," HumidDHT ",ln2Col);
}

void varBoth()
{
  showVal(bme.readTemperature(), 18,0+24, 4,"'$",v1Col);
  showVal(bme.readHumidity(), 30+120,0+24, 4,"%",v2Col);
  showVal(bme.readPressure()/100.0, 14,82+24, 6,NULL,v3Col);
  showVal(bme.readAltitude(SEA_LEVEL_PRESSURE), 21+120,82+24, 5,NULL,v4Col);
  font.setFont(Term9x14); 
  font.printStr(xe+2,82+22+14,"m");

  int ret = readDHT11(DHT11_PIN);   // only positive values - room temperatures
  if(ret==DHT_OK) {
    showVal(tempDHT, 18,162+25, 4,"'$",v1aCol);
    showVal(humidDHT, 30+120,162+25, 4,"%",v2aCol);
  }
}

void loop()
{
  if(mode!=lastMode) {
    lastMode=mode;
    lcd.fillScreen(BLACK);
    if(mode) constBoth(); else constBME280();
  }

  if(millis()-ms>1000) {
    ms = millis();
    if(mode) varBoth(); else varBME280();
  }

  int st = checkButton();
  if(st>0) { if(++mode>1) mode=0; }
}

