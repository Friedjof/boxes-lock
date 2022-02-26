// Friedjof Noweck
// 13.02.2022 So

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32_Servo.h>
#include <SPI.h>
#include <MFRC522.h>

#define ledPin 22

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
void boxLighting(int index, uint32_t color);
void boxLightingSpecial(uint32_t color);
void clearLED();
char auth();

char ledRange[4][2] = {{0, 19}, {19, 30}, {30, 50}, {50, 60}};

char step = 0;
char colorStatus = 1;
char effect = 0;

// RFID UID
char myRFID_UID[4] = {0xC7, 0xD1, 0xB8, 0x79};

volatile bool cardPresent = false;

// action timer
unsigned long int actionTimer = millis();

// {x, x, x, x, x, lastMagnetSensorStatus, cached lock status, lock status}
char mainBools = 0x04;
char mainResult = 0x00;

// Timer
unsigned long int effectTimer = millis();

Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, ledPin, NEO_GRB + NEO_KHZ800);

MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo myservo;

void setup()
{
  Serial.begin(115200);

  myservo.attach(servoPin, 500, 2400);
  
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

  // Schließe das Schlosses
  myservo.write(180);

  strip.begin();
  strip.setBrightness(50);
  strip.show();
}

void loop()
{
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
