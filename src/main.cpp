#include "claimInvoice.hpp"
#include "env.hpp"
#include "generateInvoice.hpp"
#include "lnurlwRequest.hpp"
#include "lud06Request.hpp"
#include <Adafruit_PN532_NTAG424.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>

///////////////////////////////////////////////////// NFC module //////////////////////////////////////////////////////
// define pins         // names in board
#define PN532_SCK (2)  // SCK
#define PN532_MISO (5) // M
#define PN532_MOSI (3) // TX
#define PN532_SS (4)   // RX

// define NFC module
Adafruit_PN532 nfcModule(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

////////////////////////////////////////////////// Global variables ///////////////////////////////////////////////////
String callbackLud06;

uint8_t success;
uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer to store the returned UID
uint8_t uidLength;                     // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

void setup(void) {
    Serial.begin(9600);

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
}

void loop(void) {
    Serial.printf("\nWaiting for an ISO14443A Card\n");
    success = nfcModule.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

    if (success) {
        Serial.printf("Found an ISO14443A card\n");

        if ((uidLength == 7) && nfcModule.ntag424_isNTAG424()) {
            /// Read data from the card ///
            uint8_t data[256];
            uint8_t bytesRead = nfcModule.ntag424_ISOReadFile(data);
            String callbackLnurlw;

            if (bytesRead) {
                // Dump the data
                data[bytesRead] = 0;
                String lnurlw = (char *)data;
                Serial.printf("LNURLw from card: %s\n", lnurlw.c_str());

                callbackLnurlw = getLnurlwCallback(lnurlw); // Obtain LNURLw callback
            } else {
                Serial.printf("Failed to read data\n");
            }

            /// Generate invoice ///
            String invoice = generateInvoice(callbackLud06);

            /// Claim invoice ///
            DynamicJsonDocument doc = claimInvoice(callbackLnurlw, invoice);

            serializeJsonPretty(doc, Serial);
        } else {
            Serial.printf("Not a NTAG424 card\n");
        }

        delay(1000);
    } else {
        Serial.printf("ERROR: Failed to read card\n ");
        delay(1000);
    }
}