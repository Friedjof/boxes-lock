// Friedjof Noweck
// 13.02.2022 So

#include <Arduino.h>
#include "SPIFFS.h"

#include <ArduinoJson.h>

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

// Allocate a temporary JsonDocument
// Don't forget to change the capacity to match your requirements.
// Use https://arduinojson.org/v6/assistant to compute the capacity.
StaticJsonDocument<1024> configDoc;

// Structs
struct Config {
  JsonArray defaultKeys;
  JsonArray adminKeys;
  JsonArray currentKey;
  JsonArray masterKey;
  JsonArray nullArray;
  int lastAdminKeyIndex;
};

// Prototype
void callbackRightButton(Button2& btn);
void callbackRightButtonLongClick(Button2& btn);
void callbackRightButtonDoubleClick(Button2& btn);

void callbackLeftButton(Button2& btn);
void callbackLeftButtonDoubleClick(Button2& btn);
void callbackLeftButtonLongClick(Button2& btn);

void boxLighting(int index, uint32_t color);
void boxLightingSpecial(uint32_t color);

void clearLED();
uint32_t Wheel(byte WheelPos);

void loadConfiguration(const char *filename);
void saveGlobalConfiguration();
void getCurrentKeyID();
char authorizeCard();

Config globalConfig;

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

volatile bool cardPresent = false;

// action timer
unsigned long int actionTimer = millis();
unsigned long int specialLightModeTimer = millis();

// {x, x, x, delCardIdMode, addCardIdMode, lastMagnetSensorStatus, cached lock status, lock status}
char mainBools = 0x00;
char mainResult = 0x00;


char buttonBools = 0x00;
char resultButtonBool = 0x00;

// Timer
unsigned long int effectTimer = millis();

Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, ledPin, NEO_GRB + NEO_KHZ800);

MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo myservo;

File configFile;

void setup()
{
  Serial.begin(115200);

  SPIFFS.begin(true);

  myservo.attach(servoPin, 500, 2400);

  globalConfig.adminKeys = configDoc.as<JsonArray>();
  globalConfig.currentKey = configDoc.as<JsonArray>();

  // Config Buttons

  rightButton.begin(buttonPinRight);
  rightButton.setClickHandler(callbackRightButton);
  rightButton.setLongClickHandler(callbackRightButtonLongClick);
  rightButton.setDoubleClickHandler(callbackRightButtonDoubleClick);

  leftButton.begin(buttonPinLeft);
  leftButton.setClickHandler(callbackLeftButton);
  leftButton.setDoubleClickHandler(callbackLeftButtonDoubleClick);
  leftButton.setLongClickHandler(callbackLeftButtonLongClick);
  
  rightButton.setLongClickTime(1000);
  rightButton.setDoubleClickTime(250);

  leftButton.setLongClickTime(5000);

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

  loadConfiguration("/config.json");
}

void loop()
{
  rightButton.loop();
  leftButton.loop();

  if (mainBools & 0x08 || mainBools & 0x10)
  {
    while (mainBools & 0x08 || mainBools & 0x10)
    {
      leftButton.loop();

      // new tag is available
      if (mfrc522.PICC_IsNewCardPresent())
      { 
        // NUID has been readed
        if (mfrc522.PICC_ReadCardSerial())
        {
          getCurrentKeyID();

          if (globalConfig.currentKey == globalConfig.masterKey)
          {
            // halt PICC
            mfrc522.PICC_HaltA();
            // stop encryption on PCD
            mfrc522.PCD_StopCrypto1();

            digitalWrite(redLEDpin, HIGH);
            digitalWrite(greenLEDpin, LOW);

            Serial.println("Authorization by master key.");

            // new tag is available
            while (mainBools & 0x08 || mainBools & 0x10)
            {
              leftButton.loop();

              if (mfrc522.PICC_IsNewCardPresent())
              {
                // NUID has been readed
                if (mfrc522.PICC_ReadCardSerial())
                {
                  getCurrentKeyID();

                  char notInKeys = 0x01;

                  for (int i = 0; i < globalConfig.adminKeys.size(); i++)
                  {
                    if (globalConfig.currentKey == globalConfig.adminKeys[i])
                    {
                      notInKeys = 0x00;
                    }
                  }

                  if (mainBools & 0x10)
                  {
                    notInKeys = 0x01 ^ notInKeys;
                  }

                  if (notInKeys)
                  {
                    if (0x10 ^ mainBools && 0x08 & mainBools)
                    {
                      if (globalConfig.currentKey == globalConfig.adminKeys[globalConfig.lastAdminKeyIndex])
                      { }
                      else
                      {
                        globalConfig.adminKeys.add(globalConfig.currentKey);

                        Serial.print("Add chip ");
                        serializeJson(globalConfig.currentKey, Serial);
                        Serial.println("...done.");

                        mainBools = mainBools & 0x0F7;
                        saveGlobalConfiguration();
                        digitalWrite(greenLEDpin, HIGH);
                      }
                    }
                    else if (0x10 & mainBools && 0x08 ^ mainBools)
                    {

                      if (globalConfig.currentKey == globalConfig.masterKey)
                      {
                        Serial.println("Can not remove master key.");
                      }
                      else
                      {
                        for (int index = 0; index < globalConfig.adminKeys.size(); index++)
                        {
                          if (globalConfig.currentKey == globalConfig.adminKeys[index])
                          {
                            globalConfig.adminKeys.remove(index);

                            Serial.print("Chip ");
                            serializeJson(globalConfig.currentKey, Serial);
                            Serial.println(" deleted");

                            mainBools = mainBools & 0x0EF;
                            saveGlobalConfiguration();
                            digitalWrite(greenLEDpin, HIGH);
                          }
                        }
                      }
                    }
                    
                    // halt PICC
                    mfrc522.PICC_HaltA();
                    // stop encryption on PCD
                    mfrc522.PCD_StopCrypto1();
                  }
                }
                else
                { }
              }
              else
              { }
            }
          }
          else
          {
            // halt PICC
            mfrc522.PICC_HaltA();
            // stop encryption on PCD
            mfrc522.PCD_StopCrypto1();

            Serial.println("This kay is not the master key.");

            for (int i = 0; i < 5; i++)
            {
              digitalWrite(redLEDpin, HIGH);
              delay(50);
              digitalWrite(redLEDpin, LOW);
              delay(50);
            }
          }
        }
        else
        { }
      }
      else
      { }
    }
  }

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
        strip.setPixelColor((specialLightModeCounter - snakeLenght) + specialLightModeCounterMAX, strip.Color(0, 0, 200));
      }
      else
      {
        strip.setPixelColor(specialLightModeCounter - snakeLenght, strip.Color(0, 0, 200));
      }

      // strip.setPixelColor(specialLightModeCounter, strip.Color(150, 75, 6));
      strip.setPixelColor(specialLightModeCounter, strip.Color(200, 0, 0));

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
  if (mfrc522.PICC_IsNewCardPresent() && (mainBools ^ 0x04))
  { 
    // NUID has been readed
    if (mfrc522.PICC_ReadCardSerial())
    {
      if (authorizeCard())
      {
        mainBools = 0x01 ^ mainBools;

        Serial.print("RFID: Token ");
        for (int i = 0; i < globalConfig.adminKeys.size(); i++)
        {
          if (i == globalConfig.lastAdminKeyIndex)
          {
            Serial.println(i);
          }
        }
      }
      else
      { }

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

void getCurrentKeyID()
{
  configDoc["currentKey"].clear();
  configDoc.garbageCollect();

  globalConfig.currentKey = configDoc.createNestedArray("currentKey");

  for (int i = 0; i < mfrc522.uid.size; i++) {
    globalConfig.currentKey.add((unsigned char) mfrc522.uid.uidByte[i]);
  }

  Serial.print("RFID Card: ");
  serializeJson(globalConfig.currentKey, Serial);
  Serial.println();
}

char authorizeCard()
{
  char keyInList = 0x00;

  getCurrentKeyID();

  for (int index = 0; index < globalConfig.adminKeys.size(); index++)
  {
    if (globalConfig.currentKey == globalConfig.adminKeys[index])
    {
      keyInList = 0x01;
      globalConfig.lastAdminKeyIndex = index;
    }
  }

  if (keyInList & 0x01)
  {
    return 0x01;
  }
  else
  {
    for (int i = 0; i < 5; i++)
    {
      digitalWrite(redLEDpin, LOW);
      delay(50);
      digitalWrite(redLEDpin, HIGH);
      delay(50);
    }
    return 0x00;
  }
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

  if (0x01 ^ specialLightModeRun)
  {
    Serial.println("Stop light mode");
  }
  else
  {
    Serial.println("Start light mode");
  }
}

void callbackRightButtonDoubleClick(Button2& btn)
{
  Serial.println("--- Debug: DOC Object ---");
  serializeJsonPretty(configDoc, Serial);
  Serial.println("\n-------------------------");
}

void callbackLeftButton(Button2& btn)
{
  if (mainBools ^ 0x10)
  {
    mainBools = 0x08 ^ mainBools;

    digitalWrite(redLEDpin, ((0x08 ^ mainBools) >> 0x03));

    digitalWrite(greenLEDpin, HIGH);

    if ((mainBools & 0x08) >> 0x03)
    {
      Serial.println("Start add key mode");
    }
    else
    {
      Serial.println("Stop add key mode");
    }
  }
}

void callbackLeftButtonDoubleClick(Button2& btn)
{
  if (mainBools ^ 0x08)
  {
    mainBools = 0x10 ^ mainBools;

    digitalWrite(redLEDpin, ((0x10 ^ mainBools) >> 0x04));
    digitalWrite(greenLEDpin, HIGH);

    if ((mainBools & 0x10) >> 0x04)
    {
      Serial.println("Start del key mode");
    }
    else
    {
      Serial.println("Stop del key mode");
    }
  }
}

void callbackLeftButtonLongClick(Button2& btn)
{
  globalConfig.adminKeys = globalConfig.defaultKeys;
  saveGlobalConfiguration();

  for (int i = 0; i < 5; i++)
  {
    digitalWrite(redLEDpin, LOW);
    delay(50);
    digitalWrite(redLEDpin, HIGH);
    delay(50);
  }
}

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

// Loads the configuration from a file
void loadConfiguration(const char *filename) {
  // Open file for reading
  File file = SPIFFS.open(filename, "r");

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(configDoc, file);
  if (error)
    Serial.println(F("Failed to read file, using default configuration"));

  globalConfig.adminKeys = configDoc["adminKeys"].as<JsonArray>();
  globalConfig.defaultKeys = configDoc["defaultKeys"].as<JsonArray>();
  globalConfig.currentKey = configDoc["currentKey"].as<JsonArray>();
  globalConfig.masterKey = configDoc["masterKey"].as<JsonArray>();
  globalConfig.nullArray = configDoc["nullArray"].as<JsonArray>();

  Serial.print("adminKeys: ");
  serializeJson(globalConfig.adminKeys, Serial);
  Serial.println();

  Serial.print("masterKey: ");
  serializeJson(globalConfig.masterKey, Serial);
  Serial.println();

  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();
}

void saveGlobalConfiguration()
{
  // Delete existing file, otherwise the configuration is appended to the file
  SPIFFS.remove("/config.json");

  // Open file for writing
  File file = SPIFFS.open("/config.json", FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create file"));
    return;
  }

  // Serialize JSON to file
  if (serializeJson(configDoc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  file.close();
}