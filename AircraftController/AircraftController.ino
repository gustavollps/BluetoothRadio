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
TFT_eSprite radioConnection = TFT_eSprite(&tft);
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
char data[18];

RF24 radio(13, 2);

Loop screenLoop(2);
Loop radioLoop(20);

uint8_t dataCounter = 0;
uint8_t heartBeatEvery = 10;
uint8_t heartBeatCounter = heartBeatEvery;
uint8_t failedHeartBeatsTolerance = 10;
uint8_t failedHeartBeats = failedHeartBeatsTolerance;
float connectionQuality = 0;

TimerEvent dataReceivedTimer;

void registerCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    Serial.println(F("Client Connected"));
    bluetoothConnection.fillSprite(TFT_GREEN);
    bluetoothConnection.setTextColor(TFT_WHITE, TFT_GREEN);
    bluetoothConnection.drawString("CONNECTED", 12, 4);
    bluetoothConnection.pushSprite(0, 0);
  } else if (event == ESP_SPP_CLOSE_EVT) {
    bluetoothConnection.fillSprite(TFT_RED);
    bluetoothConnection.setTextColor(TFT_WHITE, TFT_RED);
    bluetoothConnection.drawString("DISCONNECTED", 5, 4);
    bluetoothConnection.pushSprite(0, 0);
  }
}

void createSprites() {
  bluetoothConnection.createSprite(80, 15);
  radioConnection.createSprite(80, 15);
  dataReceived.createSprite(HEIGHT, 10);
  dataSent.createSprite(HEIGHT, 10);
}

void updateData() {
}

void updateConnectionCallback() {
  receivingBtData = false;
  //Serial.println("data Callback");
}

void statusDataReceived() {
}

struct bluetoothRadioMessage {
  char throttle[4];
  char yaw[4];
  char roll[4];
  char pitch[4];
  char flightMode;
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
    bluetoothMessage->throttle[i] = array[index++];    
  }
  for (int i = 0; i < 4; i++) {
    bluetoothMessage->yaw[i] = array[index++];    
  }
  for (int i = 0; i < 4; i++) {
    bluetoothMessage->roll[i] = array[index++];    
  }
  for (int i = 0; i < 4; i++) {
    bluetoothMessage->pitch[i] = array[index++];    
  }
  bluetoothMessage->flightMode = array[index];
}

void setupRadio() {
  if (!radio.begin()) {
    Serial.println(F("Radio hardware is not responding!!"));
  } else {
    Serial.println(F("Radio ok!"));
  }

  radio.setPALevel(RF24_PA_HIGH);
  radio.setChannel(115);
  radio.openWritingPipe(transmissionPipe);
  radio.openReadingPipe(1, readingPipe);
  radio.setDataRate(RF24_250KBPS);
  radio.setCRCLength(RF24_CRC_16);
  radio.stopListening();
}

void setup(void) {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  Serial.begin(115200);

  setupRadio();

  SerialBT.register_callback(registerCallback);
  SerialBT.begin("AircraftController");

  createSprites();

  dataReceivedTimer.set(2000, updateConnectionCallback);

  bluetoothConnection.fillSprite(TFT_RED);
  bluetoothConnection.setTextColor(TFT_WHITE, TFT_RED);
  //bluetoothConnection.setTextSize(1);
  bluetoothConnection.drawString("DISCONNECTED", 5, 4);
  bluetoothConnection.pushSprite(0, 0);
}

void loop() {

  dataReceivedTimer.update();

  if (screenLoop.ok()) {
    if (receivingBtData) {
      dataReceived.fillSprite(TFT_BLACK);
      dataReceived.drawString("Receiving BT data.", 0, 0);
      dataReceived.pushSprite(0, 30);
    } else {
      dataReceived.fillSprite(TFT_BLACK);
      dataReceived.pushSprite(0, 30);
    }
  }

  if (radioLoop.ok()) {
#ifdef DEBUG
    Serial.println(F("Sending data"));
#endif
    dataReady = false;
    heartBeatCounter++;
    if (heartBeatCounter < heartBeatEvery) {      
      data[sizeof(data)-1] = '?';
      if (!radio.write(data, sizeof(data))){
        setupRadio();
      }
    } 
    
    else if (heartBeatCounter == heartBeatEvery) {
      data[sizeof(data)-1] = '!';
      if (!radio.write(data, sizeof(data))){
        setupRadio();
      }
      radio.startListening();      
    } 
    
    else {
#ifdef DEBUG
      Serial.print(F("Waiting heartBeat... "));
#endif
      if (radio.available()) {
        char heartBeat[5];
        radio.read(heartBeat, sizeof(heartBeat));
        radio.stopListening();
#ifdef DEBUG
        Serial.println(F("done"));
#endif
        failedHeartBeats = 0;
      } else {
#ifdef DEBUG
        Serial.println(F("failed"));
#endif
        failedHeartBeats++;
      }
      heartBeatCounter = 0;

      if (failedHeartBeats < failedHeartBeatsTolerance) {
        radioConnection.fillSprite(TFT_GREEN);
        radioConnection.setTextColor(TFT_WHITE, TFT_GREEN);
        radioConnection.drawString("CONNECTED", 5, 4);
        radioConnection.pushSprite(150, 0);

        float newQuality = (failedHeartBeatsTolerance-failedHeartBeats)/failedHeartBeatsTolerance * 100;              
        if(connectionQuality == 0)
          connectionQuality = newQuality;
        else
          connectionQuality = connectionQuality * (0.95) + newQuality * (0.05);

        char string[40];
        sprintf(string, "Connection quality: %d %%" , int(connectionQuality));
        SerialBT.println(string);        
      } else {
        radioConnection.fillSprite(TFT_RED);
        radioConnection.setTextColor(TFT_WHITE, TFT_RED);
        radioConnection.drawString("DISCONNECTED", 5, 4);
        radioConnection.pushSprite(150, 0);
        connectionQuality = 0;
        SerialBT.println(F("No response from drone radio!"));
      }      
    }
  }

  if (SerialBT.available()) {
    char byteRead = SerialBT.read();
    //Serial.println(char(byteRead));
    if (char(byteRead) == '?') {  //end of message
      if (dataCounter == sizeof(data) - 1) {
        dataReady = true;
        extractData(data);
        
        for(int i=0; i<sizeof(data); i++){
          Serial.print(char(data[i]));
        }
        Serial.println();
        
      } else {
#ifdef DEBUG
        Serial.print(F("Broken message:"));
        for (int i = 0; i < sizeof(data); i++) {
          Serial.print(F(char(data[i])));
        }
        Serial.println();
#endif
      }
      dataCounter = 0;
    } else {
      data[dataCounter] = byteRead;
      dataCounter++;
      if (dataCounter == sizeof(data))
        dataCounter = 0;
    }

    receivingBtData = true;
    dataReceivedTimer.reset();
  }

  if (Serial.available()) {
    while (Serial.available()) {
      SerialBT.print(F(char(Serial.read())));
    }
    SerialBT.println();
  }
}
