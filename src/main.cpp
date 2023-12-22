#include "claimInvoice.hpp"
#include "env.hpp"
#include "generateInvoice.hpp"
#include "lnurlwRequest.hpp"
#include "lud06Request.hpp"
#include <Adafruit_PN532_NTAG424.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <Wire.h>

///////////////////////////////////////////////////// NFC module //////////////////////////////////////////////////////
// define pins       // names in module
#define PN532_MOSI 3 // TX
#define PN532_MISO 5 // M
#define PN532_SS 4   // RX
#define PN532_SCK 2  // SCK

// Define NFC module
Adafruit_PN532 nfcModule(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

////////////////////////////////////////////////// Global variables ///////////////////////////////////////////////////
String callbackLud06;

/////////////////////////////////////////////////////// NTP ///////////////////////////////////////////////////////////
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

/////////////////////////////////////////////////// Peripherals ///////////////////////////////////////////////////////
// LEDs
#define LED_RED 33
#define LED_YELLOW 25
#define LED_GREEN 26
#define BUZZER 27
void confirmationLed();
// Touch pins
#define TOUCH_PIN_ADD 15
#define TOUCH_PIN_SUB 13
// Screen
const uint8_t PIN_SCL = 22; // SCK
const uint8_t PIN_SDA = 21;
U8G2_SH1106_128X32_VISIONOX_F_HW_I2C screen(U8G2_R0, 4, PIN_SCL, PIN_SDA);

/////////////////////////////////////////////////////// Tasks //////////////////////////////////////////////////////////
TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;
TaskHandle_t Task4;
TaskHandle_t Task5;
void Task1code(void *vpParameters);
void Task2code(void *vpParameters);
void Task3code(void *vpParameters);
void Task4code(void *vpParameters);
void Task5code(void *vpParameters);

void setup(void) {
    Serial.begin(9600);

    /// Peripherals ///
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_YELLOW, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(BUZZER, OUTPUT);
    screen.begin();
    screen.setFont(u8g2_font_amstrad_cpc_extended_8f);
    screen.setContrast(40);
    screen.drawStr(0, 20, "Hello world!");
    screen.sendBuffer();
    delay(750);

    /// WiFi connection ///
    WiFi.begin(ENV_SSID, ENV_PASS);
    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("Connecting to WiFi");
        screen.clear();
        screen.drawStr(12, 10, "Concting WiFi");
        while (WiFi.status() != WL_CONNECTED) {
            Serial.printf(".");
            screen.drawStr(40, 20, ".     ");
            screen.drawStr(40, 30, "     .");
            screen.sendBuffer();
            delay(250);
            screen.drawStr(40, 20, "     .");
            screen.drawStr(40, 30, ".     ");
            screen.sendBuffer();
            delay(250);
        }
        Serial.printf("\nConnected to the WiFi network\n");
        screen.clear();
        screen.drawStr(4, 20, "WiFi connected!");
        screen.sendBuffer();
        delay(350);
    }

    /// NTP client ///
    WiFiUDP ntpUDP;
    NTPClient timeClient(ntpUDP, "pool.ntp.org");
    timeClient.begin();

    /// NFC module ///
    nfcModule.begin();
    uint32_t versionData = nfcModule.getFirmwareVersion();

    if (!versionData) {
        Serial.printf("Didn't find PN5xx board");
        screen.clear();
        screen.drawStr(0, 20, "Not detected NFC");
        screen.sendBuffer();
        while (1) {
            ; // stop
        }
    }

    // Print out chip version and firmware version
    Serial.printf("Found chip PN5");
    Serial.printf("%x\n", (versionData >> 24) & 0xFF);
    Serial.printf("Firmware ver. ");
    Serial.printf("%d", (versionData >> 16) & 0xFF);
    Serial.printf(".");
    Serial.printf("%d\n", (versionData >> 8) & 0xFF);

    /// Obtain LUD06 ///
    Serial.printf("Obtaining LUD06 callback\n");
    callbackLud06 = getLud06Callback(ENV_LNURL);

    // Task1code is a task to interact with card
    xTaskCreatePinnedToCore(Task1code, /* Function to implement the task */
                            "Task1",   /* Name of the task */
                            10000,     /* Stack size in words */
                            NULL,      /* Task input parameter */
                            0,         /* Priority of the task */
                            &Task1,    /* Task handle. */
                            0);        /* Core where the task should run */

    // Task2code is a task to generate invoice
    xTaskCreatePinnedToCore(Task2code, "Task2", 10000, NULL, 0, &Task2, 0);

    // Task3code is a task of status LED
    xTaskCreatePinnedToCore(Task3code, "Task3", 10000, NULL, 0, &Task3, 0);

    // Task4code is a task to adjust amount
    xTaskCreatePinnedToCore(Task4code, "Task4", 10000, NULL, 0, &Task4, 0);

    // Task5code is a task of confirmation card read
    xTaskCreatePinnedToCore(Task5code, "Task5", 10000, NULL, 0, &Task5, 0);
}

uint8_t data[256];
uint8_t bytesRead;
String callbackLnurlw;
String invoice;
String invoiceStatus;
bool invoiceWait = false;
String amount;

void loop(void) {
    Serial.printf("\nloop running on core %d\n", xPortGetCoreID());
    vTaskResume(Task4);
    amount = "1000"; // reset amount
    Serial.printf("Waiting for an ISO14443A Card\n");
    // digitalWrite(LED_YELLOW, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    screen.clear();
    screen.drawStr(0, 10, "Invoice amount:");
    screen.drawStr(48, 20, ((String)(amount.toInt() / 1000)).c_str());
    screen.drawStr(0, 32, "Tap card to pay");
    screen.sendBuffer();

    uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer to store the returned UID
    uint8_t uidLength;                     // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    uint8_t success = nfcModule.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

    invoiceStatus = "";
    if (success) {
        Serial.printf("Found an ISO14443A card\n");
        vTaskSuspend(Task4);

        if ((uidLength == 7) && nfcModule.ntag424_isNTAG424()) { // Read data from the card if it is a NTAG424 card
            bytesRead = nfcModule.ntag424_ISOReadFile(data);
            invoiceWait = true;

            if (bytesRead) {
                vTaskResume(Task5);

                Serial.printf("Start time\n");
                unsigned long startTime = timeClient.getEpochTime();

                vTaskResume(Task1);
                vTaskResume(Task2);
                // vTaskResume(Task3);

                while (eTaskGetState(Task1) != eSuspended || eTaskGetState(Task2) != eSuspended) {
                    delay(10);
                }

                DynamicJsonDocument doc = claimInvoice(callbackLnurlw, invoice); // Claim invoice
                invoiceWait = false;

                unsigned long endTime = timeClient.getEpochTime();
                Serial.printf("\nTime elapsed: %d seconds\n", endTime - startTime);

                serializeJsonPretty(doc, Serial); // Print the result after  paying the invoice (or try to)
                invoiceStatus = doc["status"].as<String>();
            } else {
                Serial.printf("Failed to read data\n");
            }
        } else {
            Serial.printf("Not a NTAG424 card\n");
            delay(500);
        }
    } else {
        Serial.printf("ERROR: Failed to read card\n ");
        delay(500);
    }

    if (invoiceStatus == "OK") {
        delay(250);
        screen.clear();
        screen.drawStr(44, 20, "Paid!");
        screen.sendBuffer();

        int amountSats = amount.toInt() / 1000;
        do {
            digitalWrite(LED_GREEN, HIGH);
            digitalWrite(BUZZER, HIGH);
            delay(250);
            digitalWrite(LED_GREEN, LOW);
            digitalWrite(BUZZER, LOW);
            delay(250);
        } while (amountSats /= 10);
    } else {
        // digitalWrite(LED_YELLOW, LOW);
        delay(250);
        screen.clear();
        screen.drawStr(40, 10, "ERROR!");
        screen.drawStr(28, 20, "Try again");
        screen.sendBuffer();
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_RED, HIGH);
        digitalWrite(BUZZER, HIGH);
        delay(1500);
        digitalWrite(LED_RED, LOW);
        digitalWrite(BUZZER, LOW);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("Connecting to WiFi");
        screen.clear();
        screen.drawStr(2, 10, "WiFi Disconnect");
        while (WiFi.status() != WL_CONNECTED) {
            screen.drawStr(40, 20, ".     ");
            screen.drawStr(40, 30, "     .");
            screen.sendBuffer();
            delay(500);
            screen.drawStr(40, 20, "     .");
            screen.drawStr(40, 30, ".     ");
            screen.sendBuffer();
            Serial.printf(".");
        }
        Serial.printf("\nConnected to the WiFi network\n");
    }

    delay(500);
}

// Task5code is a task of confirmation card read
void Task5code(void *vpParameters) {
    Serial.printf("Task5 suspend\n");
    vTaskSuspend(Task5); // Suspend the task when it is created
    for (;;) {
        digitalWrite(LED_GREEN, LOW);
        delay(100);
        digitalWrite(LED_GREEN, HIGH);
        digitalWrite(BUZZER, HIGH);
        delay(100);
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(BUZZER, LOW);
        delay(100);
        digitalWrite(LED_GREEN, HIGH);
        digitalWrite(BUZZER, HIGH);
        delay(100);
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(BUZZER, LOW);
        screen.clear();
        screen.drawStr(24, 20, "Read card!");
        screen.sendBuffer();
        delay(500);

        vTaskResume(Task3);
        vTaskSuspend(Task5);
    }
}

// Task1code is a task to interact with card
void Task1code(void *vpParameters) {
    Serial.printf("Task1 suspend\n");
    vTaskSuspend(Task1); // Suspend the task when it is created
    for (;;) {
        Serial.printf("Task1 running on core %d\n", xPortGetCoreID());

        unsigned long startTime = timeClient.getEpochTime();

        /// Dump the data ///
        data[bytesRead] = 0;
        String lnurlw = (char *)data;
        Serial.printf("LNURLw from card: %s\n", lnurlw.c_str());

        callbackLnurlw = getLnurlwCallback(lnurlw); // Obtain LNURLw callback

        unsigned long endTime = timeClient.getEpochTime();
        Serial.printf("\nTime elapsed in Task1: %d seconds\n", endTime - startTime);

        vTaskSuspend(Task1);
    }
}

// Task2code is a task to generate invoice
void Task2code(void *vpParameters) {
    Serial.printf("Task2 suspend\n");
    vTaskSuspend(Task2); // Suspend the task when it is created
    for (;;) {
        Serial.printf("Task2 running on core %d\n", xPortGetCoreID());

        unsigned long startTime = timeClient.getEpochTime();

        // Serial.printf("Invoice amount: %s\n", amount.c_str());
        invoice = generateInvoice(callbackLud06, amount); // Generate invoice

        unsigned long endTime = timeClient.getEpochTime();
        Serial.printf("\nTime elapsed in Task2: %d seconds\n", endTime - startTime);

        vTaskSuspend(Task2);
    }
}

// Task3code is a task of status LED
void Task3code(void *vpParameters) {
    Serial.printf("Task3 suspend\n");
    vTaskSuspend(Task3); // Suspend the task when it is created
    for (;;) {
        Serial.printf("Task3 running on core %d\n", xPortGetCoreID());
        screen.clear();
        screen.drawStr(32, 10, "Thinking");
        screen.sendBuffer();

        uint8_t delayTime = 250;
        while (invoiceWait || eTaskGetState(Task1) != eSuspended || eTaskGetState(Task2) != eSuspended) {
            digitalWrite(LED_YELLOW, HIGH);
            screen.drawStr(40, 20, ".     ");
            screen.drawStr(40, 30, "     .");
            screen.sendBuffer();
            delay(delayTime);
            digitalWrite(LED_YELLOW, LOW);
            screen.drawStr(40, 20, "     .");
            screen.drawStr(40, 30, ".     ");
            screen.sendBuffer();
            delay(delayTime);
            delayTime = delayTime > 50 ? delayTime - 10 : 50;
        }

        vTaskSuspend(Task3);
    }
}

// Task4code is a task to adjust amount
void Task4code(void *vpParameters) {
    for (;;) {
        if (touchRead(TOUCH_PIN_SUB) < 40) {
            if (amount.toInt() <= 1000) {
                Serial.printf("Minimal limit!\n");
                screen.drawStr(8, 20, "Minimal limit!");
                screen.sendBuffer();
                delay(350);
                screen.drawStr(8, 20, "                      ");
                amount = 1000;
            } else {
                if (amount.toInt() == 100000) {
                    amount = amount.toInt() - 79000;
                } else {
                    amount = amount.toInt() == 21000 ? amount.toInt() - 20000 : amount.toInt() - 100000;
                }
            }
            screen.drawStr(0, 10, "Invoice amount:");
            screen.drawStr(48, 20, (((String)(amount.toInt() / 1000)) + "  ").c_str());
            screen.drawStr(0, 32, "Tap card to pay");
            screen.sendBuffer();
            int amountInt = amount.toInt() / 1000;
            Serial.printf("Amount: %d SATS\n", amountInt);
            delay(350);
        } else if (touchRead(TOUCH_PIN_ADD) < 40) {
            if (amount.toInt() == 1000) {
                amount = amount.toInt() + 20000;
            } else {
                amount = amount.toInt() == 21000 ? amount.toInt() + 79000 : amount.toInt() + 100000;
            }
            screen.drawStr(0, 10, "Invoice amount:");
            screen.drawStr(48, 20, (((String)(amount.toInt() / 1000)) + "  ").c_str());
            screen.drawStr(0, 32, "Tap card to pay");
            screen.sendBuffer();
            int amountInt = amount.toInt() / 1000;
            Serial.printf("Amount: %d SATS\n", amountInt);
            delay(350);
        }
    }
}