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
#include <WiFi.h>
#include <Wire.h>

///////////////////////////////////////////////////// NFC module //////////////////////////////////////////////////////
// define pins         // names in board
#define PN532_SCK 2  // SCK
#define PN532_MISO 5 // M
#define PN532_MOSI 3 // TX
#define PN532_SS 4   // RX

// Define NFC module
Adafruit_PN532 nfcModule(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

////////////////////////////////////////////////// Global variables ///////////////////////////////////////////////////
String callbackLud06;

/////////////////////////////////////////////////////// NTP ///////////////////////////////////////////////////////////
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

/////////////////////////////////////////////////// Peripherals ///////////////////////////////////////////////////////
#define LED_GREEN 32
#define LED_RED 33
#define BUZZER 25
#define LED_YELLOW 27
void confirmationLed();

void setup(void) {
    Serial.begin(9600);

    /// Peripherals ///
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_YELLOW, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(BUZZER, OUTPUT);

    /// WiFi connection ///
    WiFi.begin(ENV_SSID, ENV_PASS);
    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("Connecting to WiFi");
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.printf(".");
        }
        Serial.printf("\nConnected to the WiFi network\n");
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
    callbackLud06 = getLud06Callback(ENV_LNURL);

    // Task1
    xTaskCreatePinnedToCore(Task1code, /* Function to implement the task */
                            "Task1",   /* Name of the task */
                            10000,     /* Stack size in words */
                            NULL,      /* Task input parameter */
                            0,         /* Priority of the task */
                            NULL,      /* Task handle. */
                            0);        /* Core where the task should run */

    // Task2
    xTaskCreatePinnedToCore(Task2code, /* Function to implement the task */
                            "Task2",   /* Name of the task */
                            10000,     /* Stack size in words */
                            NULL,      /* Task input parameter */
                            0,         /* Priority of the task */
                            NULL,      /* Task handle. */
                            0);        /* Core where the task should run */
}

void loop(void) {
    Serial.printf("\nWaiting for an ISO14443A Card\n");
    digitalWrite(LED_YELLOW, HIGH);

    uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer to store the returned UID
    uint8_t uidLength;                     // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    uint8_t success = nfcModule.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
    String invoiceStatus = "";

    if (success) {
        Serial.printf("Found an ISO14443A card\n");

        if ((uidLength == 7) && nfcModule.ntag424_isNTAG424()) { // Read data from the card if it is a NTAG424 card
            uint8_t data[256];
            uint8_t bytesRead = nfcModule.ntag424_ISOReadFile(data);
            String callbackLnurlw;

            if (bytesRead) {
                confirmationLed();

                Serial.printf("Start time\n");
                unsigned long startTime = timeClient.getEpochTime();

                /// Dump the data ///
                data[bytesRead] = 0;
                String lnurlw = (char *)data;
                Serial.printf("LNURLw from card: %s\n", lnurlw.c_str());

                callbackLnurlw = getLnurlwCallback(lnurlw);                      // Obtain LNURLw callback
                String invoice = generateInvoice(callbackLud06);                 // Generate invoice
                DynamicJsonDocument doc = claimInvoice(callbackLnurlw, invoice); // Claim invoice

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
        int countdown = 3;
        while (countdown--) {
            digitalWrite(LED_GREEN, HIGH);
            digitalWrite(BUZZER, HIGH);
            delay(250);
            digitalWrite(LED_GREEN, LOW);
            digitalWrite(BUZZER, LOW);
            delay(250);
        }
    } else {
        digitalWrite(LED_RED, HIGH);
        digitalWrite(BUZZER, HIGH);
        delay(1500);
        digitalWrite(LED_RED, LOW);
        digitalWrite(BUZZER, LOW);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("Connecting to WiFi");
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.printf(".");
        }
        Serial.printf("\nConnected to the WiFi network\n");
    }
}

void confirmationLed() {
    digitalWrite(LED_YELLOW, LOW);
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
}

void Task1code(void *pvParameters) {
    Serial.print("Task1 running on core ");
    Serial.println(xPortGetCoreID());

    for (;;) {
        timeClient.update();
        vTaskDelay(1000);
    }
}

void Task2code(void *pvParameters) {
    Serial.print("Task2 running on core ");
    Serial.println(xPortGetCoreID());

    for (;;) {
        nfcModule.SAMConfig();
        vTaskDelay(1000);
    }
}