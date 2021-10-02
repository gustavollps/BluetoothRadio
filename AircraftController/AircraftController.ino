#include <TimerEvent.h>
#include <Loop.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <BluetoothSerial.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "DataManager.h"

//#define DEBUG
//#define DEBUG_BYTES

BluetoothSerial SerialBT;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite bluetoothConnection = TFT_eSprite(&tft);
TFT_eSprite radioConnection = TFT_eSprite(&tft);
TFT_eSprite dataReceived = TFT_eSprite(&tft);
TFT_eSprite dataRadioHz = TFT_eSprite(&tft);
TFT_eSprite dataRadioLast = TFT_eSprite(&tft);
TFT_eSprite dataSent = TFT_eSprite(&tft);
TFT_eSprite currentData = TFT_eSprite(&tft);
TFT_eSprite batteryLevel = TFT_eSprite(&tft);

// Width and height of sprite
#define WIDTH 135
#define HEIGHT 240

const uint64_t transmissionPipe = 0xE8E8F0F0E1LL;
const uint64_t readingPipe = 0xF0F0F0F0D2LL;

boolean receivingBtData = false;
bool dataReady = false;

RF24 radio(13, 2);

Loop screenLoop(2);
Loop radioLoop(50);

uint8_t dataCounter = 0;
uint8_t failedHeartBeatsTolerance = 50;  //1s without ack (50 messages)
uint8_t failedHeartBeats = failedHeartBeatsTolerance;
uint32_t lastResponse = 0;
float connectionQuality = 0;
uint8_t radioFrequency = 0;

TimerEvent dataReceivedTimer;
char data[20];
char defaultData[20] = {
  '0', '5', '0', '0',
  '0', '5', '0', '0',
  '0', '5', '0', '0',
  '0', '5', '0', '0',
  '0', '0', '1', '?'
};

int charArrayToInt(char array[4]) {
  int value = 0;
  value += (array[0] - 48) * 1000;
  value += (array[1] - 48) * 100;
  value += (array[2] - 48) * 10;
  value += (array[3] - 48);
  return value;
}

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
  dataRadioHz.createSprite(HEIGHT, 10);
  dataRadioLast.createSprite(HEIGHT, 10);
  dataSent.createSprite(HEIGHT, 10);
  batteryLevel.createSprite(HEIGHT, 10);
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
  char ch7;
  char ch8;
  char flightMode;
};

bluetoothRadioMessage *bluetoothMessage = new bluetoothRadioMessage;


bool extractData(char array[]) {
  /**
  * Data structure:
  * string: throttle (0000-1000)
  * string: yaw (0000-1000)
  * string: roll (0000-1000)
  * string: pitch (0000-1000)
  * message end point : "?"
  */
  int index = 0;
  int test = 0;
  int correctData = 0;
  for (int i = 0; i < 4; i++) {
    bluetoothMessage->throttle[i] = array[index++];
  }
  test = charArrayToInt(bluetoothMessage->throttle);
  if (test >= 0 && test <= 1000) {
    correctData++;
  }

  for (int i = 0; i < 4; i++) {
    bluetoothMessage->yaw[i] = array[index++];
  }
  test = charArrayToInt(bluetoothMessage->yaw);
  if (test >= 0 && test <= 1000) {
    correctData++;
  }

  for (int i = 0; i < 4; i++) {
    bluetoothMessage->roll[i] = array[index++];
  }
  test = charArrayToInt(bluetoothMessage->roll);
  if (test >= 0 && test <= 1000) {
    correctData++;
  }

  for (int i = 0; i < 4; i++) {
    bluetoothMessage->pitch[i] = array[index++];
  }
  test = charArrayToInt(bluetoothMessage->pitch);
  if (test >= 0 && test <= 1000) {
    correctData++;
  }

  bluetoothMessage->flightMode = array[index++];
  bluetoothMessage->ch7 = array[index++];
  bluetoothMessage->ch8 = array[index];

  if (correctData == 4) {    
    return true;
  } else {
    memcpy(data, defaultData, 18);
    return false;
  }
}

void setupRadio() {
  if (!radio.begin()) {
#ifdef DEBUG
    Serial.println(F("Radio hardware is not responding!!"));
#endif
  } else {
#ifdef DEBUG
    Serial.println(F("Radio ok!"));
#endif
  }

  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(100);

  radio.setAutoAck(true);
  radio.enableDynamicPayloads();
  radio.enableAckPayload();

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
      dataReceived.drawString("Incoming BT.", 0, 0);
      dataReceived.pushSprite(0, 30);
    } else {
      dataReceived.fillSprite(TFT_BLACK);
      dataReceived.pushSprite(0, 30);
    }
    char string[10] = { ' ' };
    sprintf(string, "Battery voltage: %.2fV", analogRead(33) / 4095.0 * 7, 05);
    batteryLevel.fillSprite(TFT_BLACK);
    batteryLevel.drawString(string, 0, 0);
    batteryLevel.pushSprite(0, 100);
    if (millis() - lastResponse < 500 && failedHeartBeats < failedHeartBeatsTolerance) {
      radioConnection.fillSprite(TFT_GREEN);
      radioConnection.setTextColor(TFT_WHITE, TFT_GREEN);
      radioConnection.drawString("CONNECTED", 5, 4);
      radioConnection.pushSprite(150, 0);
      float newQuality = constrain((float(failedHeartBeatsTolerance) - float(failedHeartBeats)) / float(failedHeartBeatsTolerance) * 100.0, 0.0, 100.0);
      if (connectionQuality == 0)
        connectionQuality = newQuality;
      else
        connectionQuality = connectionQuality * (0.95) + newQuality * (0.05);
      char string[60];
      sprintf(string, "%d\n%.2f\n%d?", int(connectionQuality), float(millis() - lastResponse) / 1000.0, int(radioFrequency));
      SerialBT.println(string);
    } else {
      radioConnection.fillSprite(TFT_RED);
      radioConnection.setTextColor(TFT_WHITE, TFT_RED);
      radioConnection.drawString("DISCONNECTED", 5, 4);
      radioConnection.pushSprite(150, 0);
      char string[60];
      sprintf(string, "%d\n%.2f\n%d?", 0, float(millis() - lastResponse) / 1000.0, 0);
      SerialBT.println(string);
    }
#ifdef DEBUG
    Serial.println(connectionQuality);
#endif

    dataRadioHz.fillSprite(TFT_BLACK);
    char stringRadioHz[15];
    sprintf(stringRadioHz, "Hz: %d", int(radioFrequency));
    dataRadioHz.drawString(stringRadioHz, 0, 0);
    dataRadioHz.pushSprite(150, 30);

    dataRadioLast.fillSprite(TFT_BLACK);
    char stringRadioLast[15];
    sprintf(stringRadioLast, "Last: %.1f", float(millis() - lastResponse) / 1000.0);
    dataRadioLast.drawString(stringRadioLast, 0, 0);
    dataRadioLast.pushSprite(150, 42);
  }

  if (radioLoop.ok()) {
#ifdef DEBUG
    Serial.println(F("Sending data"));
    for (int i = 0; i < 18; i++) {
      Serial.print(data[i]);
    }
    Serial.println();
#endif
    dataReady = false;

    data[sizeof(data) - 1] = '?';
    if (!radio.write(data, sizeof(data))) {
      setupRadio();
      connectionQuality = 0;
      failedHeartBeats++;
    } else {            
      if (radio.isAckPayloadAvailable()) {
        char heartBeat[5] = { 0 };
        char expectedBeat[5] = { '0', '1', '2', '3' };
        int matchesCounter = 0;
        radio.read(heartBeat, sizeof(heartBeat));

        //radio.stopListening();
        for (int i = 0; i < sizeof(heartBeat) - 1; i++) {
          if (heartBeat[i] == expectedBeat[i]) {
            matchesCounter++;
          }
        }
        if (matchesCounter == 4) {
          radioFrequency = heartBeat[4];
          lastResponse = millis();
          failedHeartBeats = 0;
        }
        //Serial.println("GOT PAYLOAD");
      } else {
        //Serial.println("ACK WITHOUT PAYLOAD");
      }
    }
  }

  if (SerialBT.available()) {
    char byteRead = SerialBT.read();
#ifdef DEBUG_BYTES
    Serial.print(byteRead);
#endif
    if (char(byteRead) == '?') {  //end of message
#ifdef DEBUG_BYTES
      Serial.println();
#endif
      if (dataCounter == sizeof(data) - 1) {
        if (extractData(data)) {
          dataReady = true;
        } else {
          dataReady = false;
        }
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
