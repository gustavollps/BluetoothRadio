#include <TimerEvent.h>
#include <Servo.h>
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"

#include <Loop.h>

//#define DEBUG
//#define DEBUG_RAW

Servo throttle;
Servo yaw;
Servo roll;
Servo pitch;
Servo channel5;
Servo channel6;
Servo channel7;
Servo channel8;

TimerEvent disconnection;

const uint64_t transmissionPipe = 0xE8E8F0F0E1LL;
const uint64_t receivePipe = 0xF0F0F0F0D2LL;

byte ack[5] = { '0', '1', '2', '3', '4' };

char data[20];
char defaultData[20] = {
  '0', '5', '0', '0',
  '0', '5', '0', '0',
  '0', '5', '0', '0',
  '0', '5', '0', '0',
  '0', '0', '1', '?'
};


RF24 radio(A0, A1);  //CE, CSN

struct bluetoothRadioMessage {
  char throttle[4];
  char yaw[4];
  char roll[4];
  char pitch[4];
  char flightMode;
  char ch7;
  char ch8;
  char endingPoint;
};

int charArrayToInt(char array[4]) {
  int value = 0;
  value += (array[0] - 48) * 1000;
  value += (array[1] - 48) * 100;
  value += (array[2] - 48) * 10;
  value += (array[3] - 48);
  return value;
}

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
  if(test >=0 && test <= 1000 ){
    correctData++;
  }

  for (int i = 0; i < 4; i++) {
    bluetoothMessage->yaw[i] = array[index++];      
  }
  test = charArrayToInt(bluetoothMessage->yaw);
  if(test >=0 && test <= 1000 ){
    correctData++;
  }

  for (int i = 0; i < 4; i++) {
    bluetoothMessage->roll[i] = array[index++];    
    
  }
  test = charArrayToInt(bluetoothMessage->roll);
  if(test >=0 && test <= 1000 ){
    correctData++;
  }

  for (int i = 0; i < 4; i++) {
    bluetoothMessage->pitch[i] = array[index++];            
  }
  test = charArrayToInt(bluetoothMessage->pitch);
  if(test >=0 && test <= 1000 ){
    correctData++;
  }

  bluetoothMessage->flightMode = array[index++];
  bluetoothMessage->ch7 = array[index++];
  bluetoothMessage->ch8 = array[index];

  if(correctData == 4){
    return true;        
  }
  else{    
    resetBluetoothMessage();
    return false;
  }
}

void resetBluetoothMessage(){
  char defaultValue[4] = {'0', '5','0','0'};
  for(int i=0;i<4;i++){
    bluetoothMessage->pitch[i] = defaultValue[i];
  }
  for(int i=0;i<4;i++){
    bluetoothMessage->roll[i] = defaultValue[i];
  }
  for(int i=0;i<4;i++){
    bluetoothMessage->yaw[i] = defaultValue[i];
  }
  for(int i=0;i<4;i++){
    bluetoothMessage->throttle[i] = defaultValue[i];
  }
  bluetoothMessage->flightMode  = '1';
  bluetoothMessage->ch7 = '0';
  bluetoothMessage->ch8 = '0';
}

void setupRadio() {
  if (!radio.begin()) {
    Serial.println(F("Radio hardware is not responding!!"));
  }
  else{
    Serial.println(F("Radio ok!"));
  }
  radio.setPALevel(RF24_PA_MAX);  
  radio.setChannel(100);

  radio.setAutoAck(true);
  radio.enableDynamicPayloads();
  radio.enableAckPayload();

  radio.openWritingPipe(receivePipe);  //inverted
  radio.openReadingPipe(1, transmissionPipe);
  radio.setDataRate(RF24_250KBPS);
  radio.setCRCLength(RF24_CRC_16);
  
  radio.writeAckPayload(1, &ack, sizeof(ack));  
  radio.startListening();
}

void disconnectAction() {
  throttle.writeMicroseconds(900);
  setupRadio();
}

void setup(void) {
  Serial.begin(115200);
  Serial.println(F("USB connected."));

  setupRadio();

  throttle.attach(4);  //4-A2
  yaw.attach(5);       //5-A3
  roll.attach(2);      //2-A0
  pitch.attach(3);     //3-A1
  channel5.attach(6);
  channel6.attach(7);
  channel7.attach(8);
  channel8.attach(9);
  channel5.writeMicroseconds(1500);

  disconnection.set(500, disconnectAction);
}

uint32_t oldTime = millis();
int dataCounter = 0;

void loop() {  
  disconnection.update();
  if (radio.available()) {
    //TODO change ack value for some feedback    
    radio.writeAckPayload(1, &ack, sizeof(ack));

    dataCounter++;
    if (millis() - oldTime > 1000) {
#ifdef DEBUG      
      Serial.println(dataCounter);
#endif      
      ack[4] = uint8_t(constrain(dataCounter,0,255));
      dataCounter = 0;
      oldTime+=1000;
    }
    disconnection.reset();
    uint8_t bytes = radio.getDynamicPayloadSize();  // get the size of the payload
    
    radio.read(data, sizeof(data));
    

#ifdef DEBUG_RAW
    for (int i = 0; i < sizeof(data); i++) {
      Serial.print(data[i]);
    }
    Serial.println();
#endif

    if (extractData(data)) {

#ifdef DEBUG
      Serial.print(F("Throttle: "));
      Serial.print(charArrayToInt(bluetoothMessage->throttle));
      Serial.print(F("Yaw: "));
      Serial.print(charArrayToInt(bluetoothMessage->yaw));
      Serial.print(F("Roll: "));
      Serial.print(charArrayToInt(bluetoothMessage->roll));
      Serial.print(F("Pitch: "));
      Serial.print(charArrayToInt(bluetoothMessage->pitch));
      Serial.print("Flight mode: ");
      Serial.print(char(bluetoothMessage->flightMode));
      Serial.println();
#endif

      throttle.writeMicroseconds(1000 + charArrayToInt(bluetoothMessage->throttle));
      yaw.writeMicroseconds(1000 + charArrayToInt(bluetoothMessage->yaw));
      roll.writeMicroseconds(1000 + charArrayToInt(bluetoothMessage->roll));
      pitch.writeMicroseconds(1000 + charArrayToInt(bluetoothMessage->pitch));
      switch (bluetoothMessage->flightMode) {
        case '1':
          channel5.writeMicroseconds(1000);
          break;
        case '2':
          channel5.writeMicroseconds(1300);
          break;
        case '3':
          channel5.writeMicroseconds(1400);
          break;
        case '4':
          channel5.writeMicroseconds(1550);
          break;
        case '5':
          channel5.writeMicroseconds(1700);
          break;
        case '6':
          channel5.writeMicroseconds(1900);
          break;        
        default:
          channel5.writeMicroseconds(1000);
          break;
      }
      if(bluetoothMessage->ch7 == '1')
        channel7.writeMicroseconds(2000);
      else
        channel7.writeMicroseconds(1000);

      if(bluetoothMessage->ch8 == '1')
        channel8.writeMicroseconds(2000);
      else
        channel8.writeMicroseconds(1000);
      
    }
  }
}
