#include <TimerEvent.h>
#include <Servo.h>
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"

#include <Loop.h>

//#define DEBUG

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

RF24 radio(7, 8); //CE, CSN

struct bluetoothRadioMessage {
  char throttle[4];
  char yaw[4];
  char roll[4];
  char pitch[4];
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

bool extractData(byte array[]) {
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
  if (array[index] == '?')
    return true;
  else if(array[index] == '!'){    
    Serial.println("Received hearbeat request");
    sendHeartBeat();  
    return true;  
  }
  else
    return false;
}

void setupRadio() {
  if (!radio.begin()) {
    Serial.println(F("Radio hardware is not responding!!"));    
  }
  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(115);
  radio.openWritingPipe(receivePipe);  //inverted
  radio.openReadingPipe(1, transmissionPipe);
  radio.setDataRate(RF24_250KBPS);
  radio.setCRCLength(RF24_CRC_16);
  radio.startListening();
}

void sendHeartBeat(){
  radio.stopListening();
  char heartbeat[5] = {'0','1','2','3','4'};
  Serial.println("Sending heartbeat");
  if(!radio.write(heartbeat,sizeof(heartbeat))){
    setupRadio();
    Serial.println("Failed to send heartbeat");  
  }  
  radio.startListening();
}

void disconnectAction(){
  throttle.writeMicroseconds(900);
}

void setup(void) {
  Serial.begin(115200);
  Serial.println("usb connected");

  setupRadio();

  throttle.attach(4);//4-A2
  yaw.attach(5);//5-A3
  roll.attach(2);//2-A0
  pitch.attach(3);//3-A1
  channel5.attach(6);
  channel6.attach(A0);
  channel7.attach(A1);
  channel8.attach(A2);  

  disconnection.set(500,disconnectAction);
}

void loop() {
  byte data[17];
  disconnection.update();
  if (radio.available()) {
    disconnection.reset();
    radio.read(data, sizeof(data));

#ifdef DEBUG        
    for(int i=0;i<sizeof(data);i++){
      Serial.print(char(data[i]));
    }
    Serial.println();
#endif  

    if (extractData(data)) {

#ifdef DEBUG
      Serial.print("Throttle: ");
      Serial.print(charArrayToInt(bluetoothMessage->throttle));
      Serial.print("Yaw: ");
      Serial.print(charArrayToInt(bluetoothMessage->yaw));
      Serial.print("Roll: ");
      Serial.print(charArrayToInt(bluetoothMessage->roll));
      Serial.print("Pitch: ");
      Serial.print(charArrayToInt(bluetoothMessage->pitch));
      Serial.println();
#endif

      throttle.writeMicroseconds(1000 + charArrayToInt(bluetoothMessage->throttle));
      yaw.writeMicroseconds(1000 + charArrayToInt(bluetoothMessage->yaw));
      roll.writeMicroseconds(1000 + charArrayToInt(bluetoothMessage->roll));
      pitch.writeMicroseconds(1000 + charArrayToInt(bluetoothMessage->pitch));
    }
  }
}
