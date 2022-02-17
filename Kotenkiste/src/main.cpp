// Friedjof Noweck
// 13.02.2022 So

#include <Arduino.h>
#include <ESP32_Servo.h>
#include <SPI.h>
#include <MFRC522.h>

char auth();
void print_wakeup_reason();

// Servo Pin GPIO 18
#define servoPin 17

// ESP32 pin GPIO 5
#define SS_PIN  5
// ESP32 pin GPIO 27
#define RST_PIN 27

// Deep Sleep
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  60

RTC_DATA_ATTR int bootCount = 0;

// RFID UID
char myRFID_UID[4] = {0xC7, 0xD1, 0xB8, 0x79};

// Servo timer
//unsigned long int servoMotorTimer = millis();

// {x, x, x, x, x, cached lock status, lock status}
char mainBools = 0x00;

MFRC522 rfid(SS_PIN, RST_PIN);
Servo myservo;

void setup() {
  myservo.attach(servoPin, 500, 2400);

  Serial.begin(115200);
  delay(1000);

  bootCount++;
  Serial.println("Boot number: " + String(bootCount));
  print_wakeup_reason();

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");

  // init SPI bus
  SPI.begin();
  // init MFRC522
  rfid.PCD_Init();

  myservo.write(180);

  Serial.println("Going to sleep now");
  Serial.flush(); 
  esp_deep_sleep_start();
}

void loop()
{
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
  if (rfid.PICC_IsNewCardPresent())
  {
    // NUID has been readed
    if (rfid.PICC_ReadCardSerial())
    {
      if (auth())
      {
        mainBools = 0x01 ^ mainBools;
      }
      else
      {
        mainBools = mainBools & 0xFE;
      }

      Serial.println(auth(), HEX);

      // halt PICC
      rfid.PICC_HaltA();
      // stop encryption on PCD
      rfid.PCD_StopCrypto1();
    }
    else
    { }
  }
  else
  { }
}

char auth()
{
    for (int i = 0; i < rfid.uid.size; i++) {
      if (myRFID_UID[i] != rfid.uid.uidByte[i])
      {
        return 0x00;
      }
    }
    return 0x01;
}

void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }

  myservo.write(20);
}