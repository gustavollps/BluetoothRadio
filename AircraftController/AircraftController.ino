#include <TimerEvent.h>
#include <Loop.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <BluetoothSerial.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "DataManager.h"

#define DEBUG 

BluetoothSerial SerialBT;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite bluetoothConnection = TFT_eSprite(&tft);
TFT_eSprite dataReceived = TFT_eSprite(&tft);
TFT_eSprite dataSent = TFT_eSprite(&tft);
TFT_eSprite currentData = TFT_eSprite(&tft);

// Width and height of sprite
#define WIDTH 135
#define HEIGHT 240

const uint64_t transmissionPipe = 0xE8E8F0F0E1LL;
const uint64_t readingPipe = 0xF0F0F0F0D2LL;

boolean receivingBtData = false;
bool dataReady = false;
char data[17];

RF24 radio(13, 2);

Loop screenLoop(2);
Loop radioLoop(50);

TimerEvent dataReceivedTimer;

void registerCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    Serial.println("Client Connected");
    bluetoothConnection.fillSprite(TFT_GREEN);
    bluetoothConnection.setTextColor(TFT_WHITE, TFT_GREEN);
    bluetoothConnection.drawString("CONNECTED", 12, 4);
    bluetoothConnection.pushSprite(0,0);
  }
  else if(event == ESP_SPP_CLOSE_EVT){
    bluetoothConnection.fillSprite(TFT_RED);
    bluetoothConnection.setTextColor(TFT_WHITE, TFT_RED);
    bluetoothConnection.drawString("DISCONNECTED", 5, 4);
    bluetoothConnection.pushSprite(0, 0);
  }
}

void createSprites() {
  bluetoothConnection.createSprite(80, 15);
  dataReceived.createSprite(HEIGHT, 10);
  dataSent.createSprite(HEIGHT, 10);
}

void updateData() {
  
}

void updateConnectionCallback(){  
  receivingBtData = false;
  Serial.println("data Callback");
}

void statusDataReceived() {

}

struct bluetoothRadioMessage {
  char throttle[4];
  char yaw[4];
  char roll[4];
  char pitch[4];  
};

bluetoothRadioMessage *bluetoothMessage = new bluetoothRadioMessage;

void extractData(char array[]) {
  /**
  * Data structure:
  * string: throttle (0000-1000)
  * string: yaw (0000-1000)
  * string: roll (0000-1000)
  * string: pitch (0000-1000)
  * message end point : "?"
  */
  int index = 0;
  for (int i = 0; i < 4; i++) {
    bluetoothMessage->throttle[i] = array[index];
    index++;
  }
  for (int i = 0; i < 4; i++) {
    bluetoothMessage->yaw[i] = array[index];
    index++;
  }
  for (int i = 0; i < 4; i++) {
    bluetoothMessage->roll[i] = array[index];
    index++;
  }
  for (int i = 0; i < 4; i++) {
    bluetoothMessage->pitch[i] = array[index];
    index++;
  }  
}

void setup(void) {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  radio.begin();
  radio.openWritingPipe(transmissionPipe);
  radio.openReadingPipe(1, readingPipe);
  radio.setDataRate(RF24_250KBPS);
  radio.setCRCLength(RF24_CRC_16);
  radio.stopListening();

  SerialBT.register_callback(registerCallback);
  SerialBT.begin("AircraftController");

  createSprites();

  dataReceivedTimer.set(2000, updateConnectionCallback);

  bluetoothConnection.fillSprite(TFT_RED);
  bluetoothConnection.setTextColor(TFT_WHITE, TFT_RED);
  bluetoothConnection.drawString("DISCONNECTED", 5, 4);
  bluetoothConnection.pushSprite(0, 0);

  Serial.begin(115200);
}

void loop() {
  uint8_t dataCounter = 0;
  dataReceivedTimer.update();

  if (screenLoop.ok()) {    
    if(receivingBtData){
      dataReceived.fillSprite(TFT_BLACK);      
      dataReceived.drawString("Receiving BT data.", 0,0);
      dataReceived.pushSprite(0, 30);
    }
    else{
      dataReceived.fillSprite(TFT_BLACK);
      dataReceived.pushSprite(0,30);
    }
  }

  if (radioLoop.ok()) {    
    if(dataReady){      
#ifdef DEBUG
      Serial.println("Sending data"); 
#endif    
      data[16] = '?';
      radio.write(data, sizeof(data));
      dataReady = false;
    }
  }
  
  while (SerialBT.available()) {    
    char byteRead = SerialBT.read();
    if(byteRead == '?'){ //end of message
      if(dataCounter == sizeof(data)-1){
        dataReady = true;
        extractData(data);       
      }  
      else{
        Serial.println("Broken message!");        
      }
      dataCounter = 0;
    }
    else{
      data[dataCounter] = byteRead;
      dataCounter++;
      if(dataCounter == sizeof(data))
        dataCounter = 0;      
    }
            
    receivingBtData = true;
    dataReceivedTimer.reset();
  }

  if(Serial.available()){  
    while(Serial.available()) {
      SerialBT.print(char(Serial.read()));
    }
    SerialBT.println();
  }
}
