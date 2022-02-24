// Friedjof Noweck
// 13.02.2022 So

#include <Arduino.h>
#include <ESP32_Servo.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_NeoPixel.h>


char auth();

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

#define neoPixelPIN 15

// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, neoPixelPIN, NEO_GRB + NEO_KHZ800);


// RFID UID
char myRFID_UID[4] = {0xC7, 0xD1, 0xB8, 0x79};

volatile bool cardPresent = false;

// action timer
unsigned long int actionTimer = millis();

// NeoPixel timer
unsigned long int neoPixelTimer = millis();

// {x, x, x, x, x, x, cached lock status, lock status}
char mainBools = 0x00;
char mainResult = 0x00;

MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo myservo;

// Prototypen
void neoPixel();

void isr()
{
  cardPresent = true;
}

void setup() {
  myservo.attach(servoPin, 500, 2400);

  // esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, FALLING);

  // This initializes the NeoPixel library.
  pixels.begin();

  Serial.begin(115200);

  Serial.println("-- Start --");

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
  attachInterrupt(digitalPinToInterrupt(IRQ_PIN), isr, FALLING);

  // Schließe das Schlosses
  myservo.write(180);
}

void loop()
{
  if (millis() - actionTimer > (time2sleep * 1000) && 0x00)
  {
    Serial.println(">> Sleep");
    esp_deep_sleep_start();
  }
  else
  { }

  if (millis() - neoPixelTimer > 200)
  {
    neoPixel();
    neoPixelTimer = millis();
  }

  if (cardPresent)
  {
    Serial.println(">> interrupt");
    // Clear interrupts
    mfrc522.PCD_WriteRegister(MFRC522::ComIrqReg, 0x80);
    cardPresent = false;
  }
  else
  { }

  mainResult = digitalRead(IRQ_PIN);

  if (mainResult ^ ((mainBools & 0x04) >> 0x02))
  {
    Serial.println(">> trigger");

    mainBools = 0x04 ^ mainBools;
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

void neoPixel()
{
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      for (int k = 0; k < 2; k++) {
        // Moderately bright green color.
        pixels.setPixelColor(0, pixels.Color(i * 255, j * 255, k * 255));
        // This sends the updated pixel color to the hardware.
        pixels.show();
      }
    }
  }
}