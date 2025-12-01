/*
  Copyright 2022 HadesVR
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,INCLUDING BUT NOT LIMITED TO
  THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  Modified 2025 - CNCDan
  Added support for RGB Led using FastIMU Library
*/

#include <Wire.h>
#include <EEPROM.h>
#include <SPI.h>
#include "RF24.h"
#include "HID.h"
#include "FastIMU.h"
#include "FastLED.h"

//==========================================================================================================
//************************************ USER CONFIGURABLE STUFF HERE ****************************************
//==========================================================================================================

#define IMU_ADDRESS     0x68                // You can find it out by using the IMUIdentifier example
MPU6500 IMU;                                // IMU type
#define COMMON_CATHODE                      // Uncomment this if your LED is common cathode
#define USE_RF                              // Comment this to disable RF functionality.
#define IMU_GEOMTERY 4                      // Change to your current IMU geomtery (check FastIMU docs for a reference pic).
#define CALIBRATE_FLAT                      // If left uncommented accelerometer/gyro calibration must be done with the flat side of the board pointing down.
//#define SERIAL_DEBUG                        // Uncomment this to make the serial port spicy.

#define NUM_LEDS 1
#define DATA_PIN 7
CRGB leds[NUM_LEDS];

//==========================================================================================================
//************************************ DATA TRANSPORT LAYER ************************************************
//==========================================================================================================

static const uint8_t USB_HID_Descriptor[] PROGMEM = {

  0x06, 0x03, 0x00,   // USAGE_PAGE (vendor defined)
  0x09, 0x00,         // USAGE (Undefined)
  0xa1, 0x01,         // COLLECTION (Application)
  0x15, 0x00,         //   LOGICAL_MINIMUM (0)
  0x26, 0xff, 0x00,   //   LOGICAL_MAXIMUM (255)
  0x85, 0x01,         //   REPORT_ID (1)
  0x75, 0x08,         //   REPORT_SIZE (16)
  0x95, 0x3f,         //   REPORT_COUNT (1)
  0x09, 0x00,         //   USAGE (Undefined)
  0x81, 0x02,         //   INPUT (Data,Var,Abs) - to the host
  0xc0
};

//==========================================================================================================
//************************************* Data packet stuff *************************************************
//==========================================================================================================
struct HMDRAWPacket
{
  uint8_t  PacketID;

  int16_t AccX;
  int16_t AccY;
  int16_t AccZ;

  int16_t GyroX;
  int16_t GyroY;
  int16_t GyroZ;

  int16_t MagX;
  int16_t MagY;
  int16_t MagZ;

  uint16_t HMDData;

  int16_t tracker1_QuatW;
  int16_t tracker1_QuatX;
  int16_t tracker1_QuatY;
  int16_t tracker1_QuatZ;
  uint8_t tracker1_vBat;
  uint8_t tracker1_data;

  int16_t tracker2_QuatW;
  int16_t tracker2_QuatX;
  int16_t tracker2_QuatY;
  int16_t tracker2_QuatZ;
  uint8_t tracker2_vBat;
  uint8_t tracker2_data;

  int16_t tracker3_QuatW;
  int16_t tracker3_QuatX;
  int16_t tracker3_QuatY;
  int16_t tracker3_QuatZ;
  uint8_t tracker3_vBat;
  uint8_t tracker3_data;

};
struct ControllerPacket
{
  uint8_t PacketID;
  int16_t Ctrl1_QuatW;
  int16_t Ctrl1_QuatX;
  int16_t Ctrl1_QuatY;
  int16_t Ctrl1_QuatZ;
  int16_t Ctrl1_AccelX;
  int16_t Ctrl1_AccelY;
  int16_t Ctrl1_AccelZ;
  uint16_t Ctrl1_Buttons;
  uint8_t Ctrl1_Trigger;
  int8_t Ctrl1_axisX;
  int8_t Ctrl1_axisY;
  int8_t Ctrl1_trackY;
  uint8_t Ctrl1_vBat;
  uint8_t Ctrl1_THUMB;
  uint8_t Ctrl1_INDEX;
  uint8_t Ctrl1_MIDDLE;
  uint8_t Ctrl1_RING;
  uint8_t Ctrl1_PINKY;
  uint8_t Ctrl1_AnalogGrip;
  uint16_t Ctrl1_Data;

  int16_t Ctrl2_QuatW;
  int16_t Ctrl2_QuatX;
  int16_t Ctrl2_QuatY;
  int16_t Ctrl2_QuatZ;
  int16_t Ctrl2_AccelX;
  int16_t Ctrl2_AccelY;
  int16_t Ctrl2_AccelZ;
  uint16_t Ctrl2_Buttons;
  uint8_t Ctrl2_Trigger;
  int8_t Ctrl2_axisX;
  int8_t Ctrl2_axisY;
  int8_t Ctrl2_trackY;
  uint8_t Ctrl2_vBat;
  uint8_t Ctrl2_THUMB;
  uint8_t Ctrl2_INDEX;
  uint8_t Ctrl2_MIDDLE;
  uint8_t Ctrl2_RING;
  uint8_t Ctrl2_PINKY;
  uint8_t Ctrl2_AnalogGrip;
  uint16_t Ctrl2_Data;
};

static HMDRAWPacket HMDRawData;
static ControllerPacket ContData;
//==========================================================================================================
//**************************************** RF Data stuff ***************************************************
//==========================================================================================================
const uint64_t rightCtrlPipe = 0xF0F0F0F0E1LL;
const uint64_t leftCtrlPipe = 0xF0F0F0F0D2LL;

RF24 radio1(8, 10); // CE, CSN on radio 1
RF24 radio2(A1, A2); // CE, CSN on radio 2

bool newCtrlData = false;
//==========================================================================================================
//**************************************** IMU variables ***************************************************
//==========================================================================================================
AccelData IMUAccel;
GyroData IMUGyro;
MagData IMUMag;
calData calib = {0};
//==========================================================================================================

#define CALPIN  4
#define CE2     A1
#define CSN2    A2
#define CE1     8
#define CSN1    10

int ledColor = 1;
bool calPressed = false;
bool revTwoBoard = true;
bool serialOpened = false;
bool calNotDone = false;
bool addressableLED = true;

void setup() {
  //Setup Serial
  Serial.begin(115200);

  FastLED.addLeds<WS2811, DATA_PIN, GRB>(leds, NUM_LEDS);

  //Setup HID SubDescriptor
  static HIDSubDescriptor node(USB_HID_Descriptor, sizeof(USB_HID_Descriptor));
  HID().AppendDescriptor(&node);

  //Setup IO
  pinMode(7, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(A0, INPUT);
  pinMode(CALPIN, INPUT_PULLUP);

  //turn on LED
  setColor(7);

#ifdef SERIAL_DEBUG
  while (!Serial) ;
  debugPrintln("[INFO]\tSerial debug active");
#endif

  //Check board revision
  pinMode(A3, INPUT_PULLUP);
  if (digitalRead(A3)) {
    debugPrintln("[INFO]\tBoard revision: 1.X");
    //revTwoBoard = false;
  }
  else {
    debugPrintln("[INFO]\tBoard revision: 2.X");
    //revTwoBoard = true;
  }

  //Set up RF radios
#ifdef USE_RF
  //start up radio 1
  radio1.begin();
  radio1.setPayloadSize(32);
  radio1.setAutoAck(false);
  radio1.setDataRate(RF24_2MBPS);
  radio1.setPALevel(RF24_PA_LOW);
  radio1.startListening();
  //start up radio 2
  if (revTwoBoard) {
    radio2.begin();
    radio2.setPayloadSize(32);
    radio2.setAutoAck(false);
    radio2.setDataRate(RF24_2MBPS);
    radio2.setPALevel(RF24_PA_LOW);
    radio2.startListening();
  }
  if (!radio1.isChipConnected())
  {
    debugPrintln("[ERROR]\tRadio 1 Not Found!");
    errorBlink(1);    //Radio 1 is the main radio, shouldn't run without it.
  }
  else {
    debugPrintln("[INFO]\tRadio 1 Initialized");
    radio1.openReadingPipe(1, rightCtrlPipe);
    if (!revTwoBoard) {
      radio1.openReadingPipe(2, leftCtrlPipe);
    }
  }
  if (revTwoBoard) {
    if (!radio2.isChipConnected())
    {
      debugPrintln("[ERROR]\tRadio 2 Not Found!");
      debugPrintln("[INFO]\tFalling back to single radio mode");
      radio1.openReadingPipe(2, leftCtrlPipe);
    }
    else
    {
      debugPrintln("[INFO]\tRadio 2 Initialized");
      radio2.openReadingPipe(1, leftCtrlPipe);
    }
  }
#endif

  //Start up I2C
  Wire.begin();
  Wire.setClock(400000); //400khz clock

  //Initialize IMU
  IMU.setIMUGeometry(IMU_GEOMTERY);
  int err = IMU.init(calib, IMU_ADDRESS);
  if (err != 0)
  {
    debugPrint("[ERROR]\tIMU ERROR: ");
    debugPrintln(err);
    errorBlink(0);
  }
  debugPrintln("[INFO]\tIMU Initialized.");

  EEPROM.get(100, ledColor);
  if (ledColor > 6) {
    ledColor = 0;
  }
  setColor(ledColor);

  HMDRawData.PacketID = 3;
  ContData.PacketID = 2;

  int t;
  EEPROM.get(125, t);
  if (!(t == 99)) {
    debugPrintln("[INFO]\tCalibration not valid!");
    calNotDone = true;
    while (!Serial) {
      setColor(6);
      delay(500);
      setColor(7);
      delay(500);
    }
  }
  else {
    //calibration is valid
    EEPROM.get(200, calib);
    IMU.init(calib, IMU_ADDRESS);     //Reinitialize with correct calibration values
    debugPrintln("[INFO]\tLoaded Calibration from EEPROM!");
#ifdef SERIAL_DEBUG
    printCalibration();
#endif
  }
}

void loop()
{
  //run Serial
  if (Serial) {
    if (!serialOpened) {
      if (calNotDone) {
        Serial.println("[INFO]\tNo calibration data detected, do you want to enter calibration mode? (y/n)");
      } else {
        Serial.println("[INFO]\tSerial monitor open, do you want to enter calibration mode? (y/n)");
      }
    }
    serialOpened = true;
    if (Serial.read() == 'y') {
      setColor(7);
      calib = { 0 };                    //this looks important
#ifdef CALIBRATE_FLAT
      IMU.setIMUGeometry(0);
#endif
      IMU.init(calib, IMU_ADDRESS);
      Serial.println("[INFO]\tCalibrating IMU... Keep headset still on a flat and level surface...");
      delay(10000);
      IMU.calibrateAccelGyro(&calib);
      IMU.init(calib, IMU_ADDRESS);
      Serial.println("[INFO]\tAccelerometer and Gyroscope calibrated!");
      if (IMU.hasMagnetometer()) {
        delay(1000);
        Serial.println("[INFO]\tMagnetometer calibration: move IMU in figure 8 pattern until done.");
        delay(5000);
        IMU.calibrateMag(&calib);
        Serial.println("[INFO]\tMagnetic calibration done!");
      }
      Serial.println("[INFO]\tIMU Calibration complete!");
      Serial.println("[INFO]\tAccel biases X/Y/Z: ");
      Serial.print("[INFO]\t");
      Serial.print(calib.accelBias[0]);
      Serial.print(", ");
      Serial.print(calib.accelBias[1]);
      Serial.print(", ");
      Serial.println(calib.accelBias[2]);
      Serial.println("[INFO]\tGyro biases X/Y/Z: ");
      Serial.print("[INFO]\t");
      Serial.print(calib.gyroBias[0]);
      Serial.print(", ");
      Serial.print(calib.gyroBias[1]);
      Serial.print(", ");
      Serial.println(calib.gyroBias[2]);
      if (IMU.hasMagnetometer()) {
        Serial.println("[INFO]\tMag biases X/Y/Z: ");
        Serial.print("[INFO]\t");
        Serial.print(calib.magBias[0]);
        Serial.print(", ");
        Serial.print(calib.magBias[1]);
        Serial.print(", ");
        Serial.println(calib.magBias[2]);
        Serial.println("[INFO]\tMag Scale X/Y/Z: ");
        Serial.print("[INFO]\t");
        Serial.print(calib.magScale[0]);
        Serial.print(", ");
        Serial.print(calib.magScale[1]);
        Serial.print(", ");
        Serial.println(calib.magScale[2]);
      }
      setColor(4);
      Serial.println("[INFO]\tSaving Calibration values to EEPROM!");
      EEPROM.put(200, calib);
      EEPROM.put(125, 99);
      delay(1000);
      Serial.println("[INFO]\tYou can now close the Serial monitor.");
      delay(5000);
      IMU.setIMUGeometry(IMU_GEOMTERY);
      IMU.init(calib, IMU_ADDRESS);
      setColor(ledColor);
    }
  }

  //run IMU
  IMU.update();
  IMU.getAccel(&IMUAccel);
  IMU.getGyro(&IMUGyro);

  if (IMU.hasMagnetometer()) {
    IMU.getMag(&IMUMag);
    HMDRawData.MagX = (short)(IMUMag.magX * 5);
    HMDRawData.MagY = (short)(IMUMag.magY * 5);
    HMDRawData.MagZ = (short)(IMUMag.magZ * 5);
  }
  else {
    HMDRawData.MagX = (short)(0);
    HMDRawData.MagY = (short)(0);
    HMDRawData.MagZ = (short)(0);
  }

  HMDRawData.AccX = (short)(IMUAccel.accelX * 2048);
  HMDRawData.AccY = (short)(IMUAccel.accelY * 2048);
  HMDRawData.AccZ = (short)(IMUAccel.accelZ * 2048);

  HMDRawData.GyroX = (short)(IMUGyro.gyroX * 16);
  HMDRawData.GyroY = (short)(IMUGyro.gyroY * 16);
  HMDRawData.GyroZ = (short)(IMUGyro.gyroZ * 16);

  //run RF
  uint8_t pipenum;
  if (radio1.available(&pipenum)) {                  //thanks SimLeek for this idea!
    if (pipenum == 1) {
      radio1.read(&ContData.Ctrl1_QuatW, 29);        //receive right controller data
      debugPrintln("RX 1, Right Controller!");
      newCtrlData = true;
    }
    if (pipenum == 2) {
      radio1.read(&ContData.Ctrl2_QuatW, 29);        //receive left controller data
      debugPrintln("RX 1, Left Controller!");
      newCtrlData = true;
    }
  }
  if (radio2.available(&pipenum) && revTwoBoard) {
    radio2.read(&ContData.Ctrl2_QuatW, 29);        //receive left controller data
    debugPrintln("RX 2, Left controller!");
    newCtrlData = true;
  }
  if (newCtrlData) {
    HID().SendReport(1, &ContData, 63);
    newCtrlData = false;
  }

  //run LED
  if (!digitalRead(CALPIN)) {
    if (!calPressed) {
      calPressed = true;
      ledColor++;
      if (ledColor > 6) {
        ledColor = 0;
      }
      debugPrintln("[INFO]\tswitched LED color and saved new value");
      setColor(ledColor);
      EEPROM.put(100, ledColor);
      delay(40);
    }
  }
  else {
    calPressed = false;
  }
  HID().SendReport(1, &HMDRawData, 63);
}

void printCalibration()
{
  Serial.println("[INFO]\tAccel biases X/Y/Z: ");
  Serial.print("[INFO]\t");
  Serial.print(calib.accelBias[0]);
  Serial.print(", ");
  Serial.print(calib.accelBias[1]);
  Serial.print(", ");
  Serial.println(calib.accelBias[2]);
  Serial.println("[INFO]\tGyro biases X/Y/Z: ");
  Serial.print("[INFO]\t");
  Serial.print(calib.gyroBias[0]);
  Serial.print(", ");
  Serial.print(calib.gyroBias[1]);
  Serial.print(", ");
  Serial.println(calib.gyroBias[2]);
  if (IMU.hasMagnetometer()) {
    Serial.println("[INFO]\tMag biases X/Y/Z: ");
    Serial.print("[INFO]\t");
    Serial.print(calib.magBias[0]);
    Serial.print(", ");
    Serial.print(calib.magBias[1]);
    Serial.print(", ");
    Serial.println(calib.magBias[2]);
    Serial.println("[INFO]\tMag Scale X/Y/Z: ");
    Serial.print("[INFO]\t");
    Serial.print(calib.magScale[0]);
    Serial.print(", ");
    Serial.print(calib.magScale[1]);
    Serial.print(", ");
    Serial.println(calib.magScale[2]);
  }
}
void ledControl(int red, int green, int blue)
{
  if(!addressableLED){
    #ifdef COMMON_CATHODE
      digitalWrite(7, LOW);
      digitalWrite(5, red);
      digitalWrite(6, green);
      digitalWrite(9, blue);
    #else
      digitalWrite(7, HIGH);
      digitalWrite(5, !red);
      digitalWrite(6, !green);
      digitalWrite(9, !blue);
    #endif
  } else {
    leds[0] = CRGB((red * 255), (green * 255), (blue * 255));
    FastLED.show();
  }
}

void setColor(int index) {
  switch (index) {
    case 0:
      ledControl(1, 0, 0);    //red
      break;
    case 1:
      ledControl(1, 1, 0);    //yellow
      break;
    case 2:
      ledControl(0, 1, 0);    //green
      break;
    case 3:
      ledControl(0, 1, 1);    //cyan
      break;
    case 4:
      ledControl(0, 0, 1);    //blue
      break;
    case 5:
      ledControl(1, 0, 1);    //magenta
      break;
    case 6:
      ledControl(0, 0, 0);    //off
      break;
    case 7:
      ledControl(1, 1, 1);    //white
      break;
    default:
      ledControl(1, 0, 0);    //red again
      break;
  }
}

void debugPrint(String arg) {
#ifdef SERIAL_DEBUG
  Serial.print(arg);
#endif
}
void debugPrint(int arg) {
#ifdef SERIAL_DEBUG
  Serial.print(arg);
#endif
}
void debugPrintln(String arg) {
#ifdef SERIAL_DEBUG
  Serial.println(arg);
#endif
}
void debugPrintln(int arg) {
#ifdef SERIAL_DEBUG
  Serial.println(arg);
#endif
}

void errorBlink(int color)
{
  while (true)
  {
    setColor(color);
    delay(500);
    setColor(6);
    delay(500);
  }
}
