#include <SPI.h>
#include <TFT_eSPI.h>
#include <BluetoothSerial.h>
#include "nRF24L01.h"
#include "RF24.h"

BluetoothSerial SerialBT;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite bluetoothConnection = TFT_eSprite(&tft);
TFT_eSprite dataReceived = TFT_eSprite(&tft);
TFT_eSprite dataSent = TFT_eSprite(&tft);
TFT_eSprite currentData = TFT_eSprite(&tft);

// Width and height of sprite
#define WIDTH  135
#define HEIGHT 240

uint32_t time_old = 0;

const uint64_t transmissionPipe = 0xE8E8F0F0E1LL;
const uint64_t readingPipe = 0xF0F0F0F0D2LL;

boolean receivingBtData = false;

RF24 radio(13, 2);

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
  if(event == ESP_SPP_SRV_OPEN_EVT){
    Serial.println("Client Connected");
    bluetoothConnection.fillSprite(TFT_GREEN);
    bluetoothConnection.drawString("CONNECTED",  0, 0);
    bluetoothConnection.pushSprite(0, 0);
  }
}

void createSprites(){
  bluetoothConnection.createSprite(50, 10);
  dataReceived.createSprite(HEIGHT, 10);
  dataSent.createSprite(HEIGHT, 10);
}

void updateData(){

}

void statusDataReceived(){
  dataReceived.fillSprite(TFT_GREEN);
  bluetoothConnection.drawString("CONNECTED",  0, 0);
  bluetoothConnection.pushSprite(0, 0);
}

struct bluetoothRadioMessage
{
  char throttle[4];
  char yaw[4];
  char roll[4];
  char pitch[4];
  char endingPoint;
};

bluetoothRadioMessage* bluetoothMessage = new bluetoothRadioMessage;

boolean extractData(char array[]){
  /**
  * Data structure:
  * string: throttle (0000-1000)
  * string: yaw (0000-1000)
  * string: roll (0000-1000)
  * string: pitch (0000-1000)
  * message end point : "?"
  */
  int index = 0;
  for(int i=0;i<4;i++){ bluetoothMessage->throttle[i] = array[index]; index++; }
  for(int i=0;i<4;i++){ bluetoothMessage->yaw[i] = array[index]; index++; }
  for(int i=0;i<4;i++){ bluetoothMessage->roll[i] = array[index]; index++; }
  for(int i=0;i<4;i++){ bluetoothMessage->pitch[i] = array[index]; index++; }
  if(array[index] == '?')
    return true;
  else
    return false;
}

void setup(void) {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  radio.begin();
  radio.openWritingPipe(transmissionPipe);
  radio.openReadingPipe(1,readingPipe);
  radio.setDataRate(RF24_250KBPS);
  radio.setCRCLength(RF24_CRC_16);
  radio.stopListening();

  SerialBT.register_callback(callback);
  SerialBT.begin("AircraftController");

  bluetoothConnection.fillSprite(TFT_BLACK);
  bluetoothConnection.createSprite(HEIGHT, WIDTH);
  bluetoothConnection.pushSprite(0, 0);

  Serial.begin(115200);

  time_old = millis();
}

void loop() {
  //  spr.drawRect(shift, 0, 240, 135, TFT_WHITE);
  //  spr.fillRect(shift + 1, 3 + 10 * (opt - 1), 100, 11, TFT_RED);
  if(millis()-time_old > 2000){
    time_old = millis();
    receivingBtData = false;
  }

  bool done = false;
  byte volt[3];
  radio.write()
  if(radio.available()){
    radio.read( volt, sizeof(volt) );
    for(int i=0;i<3;i++){Serial.print(volt[i]); Serial.print("----");}
    Serial.println("Received message!");
  }

  while (SerialBT.available()) {
    if(!receivingBtData){
      dataReceived.fillSprite(TFT_BLACK);
      dataReceived.drawString("CONNECTED",  0, 0);
      dataReceived.pushSprite(0, 0);
      time_old = millis();
    }
    receivingBtData = true;
    Serial.print(char(SerialBT.read()));
  }
  if (Serial.available()) {
    SerialBT.print(Serial.read());
  }
}
