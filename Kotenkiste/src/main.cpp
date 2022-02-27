// Friedjof Noweck
// 13.02.2022 So

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32_Servo.h>
#include <SPI.h>
#include <MFRC522.h>
#include "Button2.h"

/*
 * Button:
 * right = 25
 * left  = 35
 * 
 * LEDs:
 * red   = 32
 * green = 26
 */

// Bottons
#define buttonPinRight 25
#define buttonPinLeft 35

// LEDs
#define redLEDpin 32
#define greenLEDpin 26

// LED Stipe data line
#define ledPin 22
#define totalLEDs 60

// Servo Pin GPIO 18
#define servoPin 17

// ESP32 pin GPIO 5
#define SS_PIN  5
// ESP32 pin GPIO 27
#define RST_PIN 27

// IRQ - RFID Interrupt
#define IRQ_PIN 26

// timer to sleep after inactivity [in secunds]
#define time2sleep 10

// Magnet Sensor
#define magnetSensorPin 4

// Prototype
void callbackRightButton(Button2& btn);
void callbackRightButtonLongClick(Button2& btn);

void callbackLeftButton(Button2& btn);
void callbackRightButtonDoubleClick(Button2& btn);

void boxLighting(int index, uint32_t color);
void boxLightingSpecial(uint32_t color);
void clearLED();
uint32_t Wheel(byte WheelPos);
char auth();

Button2 rightButton, leftButton;

char ledRange[4][2] = {{0, 20}, {20, 30}, {30, 50}, {50, 60}};

char step = 0;
char colorStatus = 1;
char effect = 0;

char lightMode = 0x00;
char specialLightMode = 0x00;
char specialLightModeRun = 0x01;
int specialLightModeCounter = 0;
int specialLightModeCounterMAX = 256;
int snakeLenght = 30;

// RFID UID
char myRFID_UID[4] = {0xC7, 0xD1, 0xB8, 0x79};

volatile bool cardPresent = false;

// action timer
unsigned long int actionTimer = millis();
unsigned long int specialLightModeTimer = millis();

// {x, x, x, x, x, lastMagnetSensorStatus, cached lock status, lock status}
char mainBools = 0x00;
char mainResult = 0x00;


char buttonBools = 0x00;
char resultButtonBool = 0x00;

// Timer
unsigned long int effectTimer = millis();

Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, ledPin, NEO_GRB + NEO_KHZ800);

MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo myservo;

void setup()
{
  Serial.begin(115200);

  myservo.attach(servoPin, 500, 2400);

  // Config Buttons
  rightButton.setLongClickTime(2000);
  rightButton.setDoubleClickTime(50);

  rightButton.begin(buttonPinRight);
  rightButton.setClickHandler(callbackRightButton);
  rightButton.setLongClickHandler(callbackRightButtonLongClick);

  leftButton.begin(buttonPinLeft);
  leftButton.setClickHandler(callbackLeftButton);
  leftButton.setDoubleClickHandler(callbackRightButtonDoubleClick);
  
  // init SPI bus
  SPI.begin();
  // init MFRC522
  mfrc522.PCD_Init();
  // Clear interrupts
  mfrc522.PCD_WriteRegister(MFRC522::ComIrqReg, 0x80);
  // Enable all interrupts
  mfrc522.PCD_WriteRegister(MFRC522::ComIEnReg, 0xA0);
  mfrc522.PCD_WriteRegister(MFRC522::DivIEnReg, 0x14);

  pinMode(IRQ_PIN, INPUT_PULLUP);
  pinMode(magnetSensorPin, INPUT_PULLUP);

  pinMode(magnetSensorPin, INPUT_PULLUP);
  pinMode(magnetSensorPin, INPUT_PULLUP);

  pinMode(redLEDpin, OUTPUT);
  pinMode(greenLEDpin, OUTPUT);

  pinMode(buttonPinRight, INPUT_PULLUP);
  pinMode(buttonPinLeft, INPUT_PULLUP);

  // Schließe das Schlosses
  myservo.write(180);

  // LEDs off
  digitalWrite(redLEDpin, HIGH);
  digitalWrite(greenLEDpin, HIGH);

  strip.begin();
  strip.setBrightness(50);
  strip.show();
}

void loop()
{
  rightButton.loop();
  leftButton.loop();

  if (specialLightMode && millis() - specialLightModeTimer > 20)
  {
    if (specialLightMode > 0x00 && (specialLightModeRun & 0x01))
    {
      if (specialLightModeCounter < specialLightModeCounterMAX)
      {
        specialLightModeCounter++;
      }
      else
      {
        specialLightModeCounter = 0;
      }
    }

    if (specialLightMode == 0x01)
    {
      for(int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + specialLightModeCounter) & 255));
      }
      strip.show();
    }
    else if (specialLightMode == 0x02)
    {
      if (specialLightModeCounter - snakeLenght < 0)
      {
        strip.setPixelColor((specialLightModeCounter - snakeLenght) + specialLightModeCounterMAX, strip.Color(0, 255, 0));
      }
      else
      {
        strip.setPixelColor(specialLightModeCounter - snakeLenght, strip.Color(0, 255, 0));
      }

      // strip.setPixelColor(specialLightModeCounter, strip.Color(150, 75, 6));
      strip.setPixelColor(specialLightModeCounter, strip.Color(255, 0, 0));

      strip.show();
    }

    specialLightModeTimer = millis();
  }

  mainResult = digitalRead(magnetSensorPin);

  if (mainResult ^ ((mainBools & 0x04) >> 0x02))
  {
    if (mainResult)
    {
      effect = 0x01;
    }
    else
    {
      effect = 0x00;
      step = 0;
      colorStatus = 1;

      specialLightMode = 0x00;
      specialLightModeCounter = 0;

      clearLED();
    }

    Serial.print("Magnet Sensor: ");
    Serial.println(mainResult, BIN);

    mainBools = 0x04 ^ mainBools;
  }

  if (effect)
  {
    boxLightingSpecial(strip.Color(50 * colorStatus, 25 * colorStatus, 2 * colorStatus));

    if (millis() - effectTimer > 50)
    {
      if (step < 15)
      {
        step++;
      }
      else
      {
        step = 0;
        if (colorStatus < 3)
        {
          colorStatus++;
        }
        else
        {
          colorStatus = 1;
          effect = 0;
        }
      }

      effectTimer = millis();
    }
  }

  if (millis() - actionTimer > (time2sleep * 1000) && 0x00)
  {
    Serial.println(">> Sleep");
    esp_deep_sleep_start();
  }
  else
  { }

  // Wenn sich der Schloss Status verändert hat:
  if ((mainBools & 0x01) ^ ((mainBools & 0x02) >> 0x01))
  {
    // auf
    if (mainBools & 0x01)
    {
      myservo.write(20);
    }
    // zu
    else if (0x01 ^ (mainBools & 0x01))
    {
      myservo.write(180);
    }
    else
    { }

    // toggle cached lock status
    mainBools = 0x02 ^ mainBools;
  }
  else
  { }

  // new tag is available
  if (mfrc522.PICC_IsNewCardPresent())
  {
    Serial.println("RFID Card present");
    // NUID has been readed
    if (mfrc522.PICC_ReadCardSerial())
    {
      if (auth())
      {
        mainBools = 0x01 ^ mainBools;

        Serial.println("RFID: Token 01");
      }
      else
      {
        mainBools = mainBools & 0xFE;
      }

      actionTimer = millis();

      // halt PICC
      mfrc522.PICC_HaltA();
      // stop encryption on PCD
      mfrc522.PCD_StopCrypto1();
    }
    else
    { }
  }
  else
  { }
}

char auth()
{
    for (int i = 0; i < mfrc522.uid.size; i++) {
      if (myRFID_UID[i] != mfrc522.uid.uidByte[i])
      {
        return 0x00;
      }
    }
    return 0x01;
}

void boxLighting(int index, uint32_t color)
{
  for (char i = ledRange[index][0]; i < ledRange[index][1]; i++)
  {
    strip.setPixelColor(i, color);
  }

  strip.show();
}

void clearLED()
{
  for (char i = 0; i < 60; i++)
  {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }

  strip.show();
}

void boxLightingSpecial(uint32_t color)
{
  strip.setPixelColor(10 + step, color);
  strip.setPixelColor(39 - step, color);
  strip.setPixelColor(40 + step, color);

  if (9 - step < 0)
  {
    strip.setPixelColor(69 - step, color);
  }
  else
  {
    strip.setPixelColor(9 - step, color);
  }

  strip.show();
}

void callbackRightButton(Button2& btn)
{
  Serial.println(">> right");
  if (((mainBools & 0x04) >> 0x02))
  {
    effect = 0x00;
    step = 0;
    colorStatus = 1;

    specialLightMode = 0x00;

    clearLED();

    if (lightMode == 0x01)
    {
      boxLighting(0, strip.Color(150, 75, 6));
      boxLighting(2, strip.Color(150, 75, 6));
    }
    else if (lightMode == 0x02)
    {
      boxLighting(1, strip.Color(150, 75, 6));
      boxLighting(3, strip.Color(150, 75, 6));
    }
    else if (lightMode == 0x03)
    {
      specialLightModeCounter = 0;
      specialLightModeCounterMAX = 256;
      specialLightMode = 0x01;
    }
    else if (lightMode == 0x04)
    {
      specialLightModeCounter = 0;
      specialLightModeCounterMAX = totalLEDs;
      specialLightMode = 0x02;
    }
  }

  if (lightMode < 0x04)
  {
    lightMode++;
  }
  else
  {
    lightMode = 0x00;
  }
}

void callbackRightButtonLongClick(Button2& btn)
{
  specialLightModeRun = 0x01 ^ specialLightModeRun;
}

void callbackLeftButton(Button2& btn)
{ }

void callbackRightButtonDoubleClick(Button2& btn)
{ }

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos)
{
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}