#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiegandNG.h>


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

WiegandNG wg;

unsigned long previousMillis = 0;        // will store last time display was updated

const long interval = 3000;           // interval at which to blank display

unsigned int bits = 0;
unsigned long fc = 0;
unsigned long cn = 0;
bool displayCardData = false;

struct cardData {
  unsigned long chunk1 = 0;
  unsigned long chunk2 = 0;
};

struct cardData DecodeWiegand(WiegandNG &tempwg) {
  volatile unsigned char *buffer=tempwg.getRawData();
  unsigned int bufferSize = tempwg.getBufferSize();
  unsigned int countedBits = tempwg.getBitCounted();

  unsigned int countedBytes = (countedBits/8);
  if ((countedBits % 8)>0) countedBytes++;
  // unsigned int bitsUsed = countedBytes * 8;

  struct cardData data;
  int bitnum = 0;
  
  for (unsigned int i=bufferSize-countedBytes; i< bufferSize;i++) {
    unsigned char bufByte=buffer[i];
    for(int x=0; x<8;x++) {
      if ( (((bufferSize-i) *8)-x) <= countedBits) {
        if((bufByte & 0x80)) {
          if(bitnum < 31) {
            data.chunk1 = data.chunk1 << 1;
            data.chunk1 |= 1;  
          } else {
            data.chunk2 = data.chunk2 << 1;
            data.chunk2 |= 1;  
          }
          Serial.print("1");          
        }
        else {
          if(bitnum < 31) {
            data.chunk1 = data.chunk1 << 1;
          } else {
            data.chunk2 = data.chunk2 << 1;  
          }
          Serial.print("0");
        }
        bitnum++;
      }
      bufByte<<=1;
    }
  }
  Serial.println();
  return data;
}

void setup() {
  Serial.begin(9600);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.clearDisplay();

  display.setTextSize(1);              // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0,0);              // Start at top-left corner
  display.println(F("Wiegand Decoder"));
  display.println(F("By: tcprst"));
  
  display.display();
  
  unsigned int pinD0 = 14;          // Wemos D1 Mini Pin D5
  unsigned int pinD1 = 12;          // Wemos D1 Mini Pin D6
  unsigned int wiegandbits = 48;
  unsigned int packetGap = 15;      // 25 ms between packet
  
  if(!wg.begin(pinD0, pinD1, wiegandbits, packetGap)) {
    Serial.println(F("Out of memory!"));
  }
  Serial.println(F("Ready..."));
}

void loop() {
  unsigned long currentMillis = millis();
  struct cardData card;
  
  if(wg.available()) {
    wg.pause();     // pause Wiegand pin interrupts
    bits=wg.getBitCounted();
    Serial.print(F("Bits="));
    Serial.println(bits); // display the number of bits counted
    Serial.print(F("RAW Binary="));
    card = DecodeWiegand(wg); // display raw data in binary form, raw data inclusive of PARITY
    wg.clear(); // compulsory to call clear() to enable interrupts for subsequent data
  }
  if(bits > 0) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(1);   
    display.print("Bits="); display.print(bits);

    switch (bits) {
      case 26:
        display.println(F(" Fmt: H10301"));
        fc = 0;
        cn = 0;
        // Facility Code
        for (int i = 24; i > 16; i--) {
          fc = fc << 1;
          if (bitRead(card.chunk1, i) == 1){
            fc |= 1;
          }
        }
        // Card number
        for (int i = 16; i > 0; i--) {
          cn = cn << 1;
          if (bitRead(card.chunk1, i) == 1){
            cn |= 1;
          } 
        }
        displayCardData = true;
        break;
      case 35:
        display.println(F(" Fmt: C1k35s"));
        fc = 0;
        cn = 0;
        // Facility Code
        for (int i = 32; i > 20; i--) {
          fc = fc << 1;
          if (bitRead(card.chunk1, i - 4) == 1){
            fc |= 1;
          }
        }
        // Card Number
        for (int i = 20; i > 0; i--) {
          cn = cn << 1;
          if (i > 3) {
            if (bitRead(card.chunk1, i - 4) == 1){
              cn |= 1;
            } 
          } else {
            if (bitRead(card.chunk2, i) == 1){
              cn |= 1;
            }            
          }
        }
        displayCardData = true;
        break;
      default:
        display.println();
        break;
    }
    if(card.chunk2 > 0) {
      display.setTextSize(2);
      display.print(card.chunk1, HEX);
      display.println(card.chunk2, HEX);
     
    } else {
      display.setTextSize(3); 
      display.println(card.chunk1, HEX);  
    }
    display.display();
    previousMillis = currentMillis;
    bits = 0;
  }
  if (currentMillis - previousMillis >= interval) {
    if (displayCardData) {
      display.clearDisplay();
      display.setCursor(0,0);
      display.setTextSize(2);   
      display.print(F("FC=")); display.println(fc);
      display.print(F("CN=")); display.println(cn);
      display.display();
      previousMillis = currentMillis;
      displayCardData = false;
    } else {
      display.clearDisplay();
      display.display();      
    }
  }
}
