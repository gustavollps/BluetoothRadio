#include <ESP32Servo.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <BluetoothSerial.h>

#define TFT_GREY 0x5AEB
BluetoothSerial SerialBT;
Servo throttle;
Servo pitch;
Servo row;
Servo yaw;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

// Width and height of sprite
#define WIDTH  135
#define HEIGHT 240

uint32_t time_old = 0;

void setup(void) {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  spr.createSprite(HEIGHT, WIDTH);
  spr.fillSprite(TFT_BLACK);
  spr.pushSprite(0, 0);

  throttle.attach(12);
  pitch.attach(13);
  row.attach(15);
  yaw.attach(2);

  SerialBT.begin("AircraftController");
  Serial.begin(115200);
  Serial.println("usb connected");

  spr.fillSprite(TFT_BLACK);
  spr.drawString("BT DATA RECEIVED",  50, 50);
  spr.pushSprite(0, 0);
  time_old = millis();
}

void loop() {
  //  spr.drawRect(shift, 0, 240, 135, TFT_WHITE);
  //  spr.fillRect(shift + 1, 3 + 10 * (opt - 1), 100, 11, TFT_RED);
  if(millis()-time_old > 500){
    time_old = millis();
  }
  
  while (SerialBT.available()) {    
    Serial.print(char(SerialBT.read()));
  }
  if (Serial.available()) {
    SerialBT.print(Serial.read());    
  }
}
