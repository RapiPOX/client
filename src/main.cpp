#include "env.hpp"
#include "extensions/tips/claimInvoice.hpp"
#include "extensions/tips/getInvoice.hpp"
#include "extensions/tips/lnurlwRequest.hpp"
#include "extensions/tips/lud06Request.hpp"
#include <Adafruit_PN532_NTAG424.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <Wire.h>

/// Debug ///
// Comment the following lines to disable debug
#define SERIALDEBUG
#define TIMEDEBUG

/// LEDs ///
#define LED_RED 33
#define LED_YELLOW 25
#define LED_GREEN 26
/// Buzzer ///
#define BUZZER 27
/// Touch pins ///
#define TOUCH_PIN_ADD 15
#define TOUCH_PIN_SUB 13
/// Screen ///
const uint8_t PIN_SCL = 22;
const uint8_t PIN_SDA = 21;
U8G2_SH1106_128X32_VISIONOX_F_HW_I2C screen(U8G2_R0, 4, PIN_SCL, PIN_SDA);

/// NFC module ///
#define PN532_MOSI 3
#define PN532_MISO 5
#define PN532_SS 4
#define PN532_SCK 2

Adafruit_PN532 nfcModule(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

/// Network Time Protocol (NTP) ///
#ifdef TIMEDEBUG
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
#endif

/// Tasks ///
TaskHandle_t Task1GetLnurlwCallback;
TaskHandle_t Task2GetInvoice;
TaskHandle_t Task3ThinkingLed;
TaskHandle_t Task4FixAmount;
TaskHandle_t Task5ConfirmationLed;
void Task1code(void *vpParameters);
void Task2code(void *vpParameters);
void Task3code(void *vpParameters);
void Task4code(void *vpParameters);
void Task5code(void *vpParameters);

/// Task parameters ///
// struct Task1Parameters {
//     String lnurlw;
// };

/// Global variables ///
String callbackLud06;
uint8_t data[256];
uint8_t bytesRead;
String callbackLnurlw;
String invoice;
String invoiceStatus;
bool invoiceWait = false;
String amount;

void setup(void) {
#ifdef SERIALDEBUG
    Serial.begin(9600);
#endif

    /// Peripherals ///
    // Init LEDs
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_YELLOW, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    // Init Buzzer
    pinMode(BUZZER, OUTPUT);
    // Init Screen
    screen.begin();
    screen.setFont(u8g2_font_amstrad_cpc_extended_8f);
    screen.setContrast(40);
    screen.drawStr(0, 20, "Hello world!");
    screen.sendBuffer();
    delay(750);

    /// Init WiFi ///
    WiFi.begin(ENV_SSID, ENV_PASS);
#ifdef SERIALDEBUG
    Serial.printf("Connecting to WiFi");
#endif
    screen.clear();
    screen.drawStr(2, 10, "Conecting WiFi");
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
#ifdef SERIALDEBUG
        Serial.printf(".");
#endif
        screen.drawStr(40, 20, ".     ");
        screen.drawStr(40, 30, "     .");
        screen.sendBuffer();
        delay(250);
        screen.drawStr(40, 20, "     .");
        screen.drawStr(40, 30, ".     ");
        screen.sendBuffer();
        delay(250);
    }
#ifdef SERIALDEBUG
    Serial.printf("\nConnected to the WiFi network\n");
#endif
    screen.clear();
    screen.drawStr(4, 20, "WiFi connected!");
    screen.sendBuffer();
    delay(350);

#ifdef TIMEDEBUG
    /// Init NTP ///
    WiFiUDP ntpUDP;
    NTPClient timeClient(ntpUDP, "pool.ntp.org");
    timeClient.begin();
#endif

    /// Init NFC ///
    nfcModule.begin();
    uint32_t versionData = nfcModule.getFirmwareVersion();

    // Check if the PN532 module is working
    if (!versionData) {
#ifdef SERIALDEBUG
        Serial.printf("Didn't find PN5xx board");
#endif
        screen.clear();
        screen.drawStr(0, 20, "Not detected NFC");
        screen.sendBuffer();
        while (!(versionData = nfcModule.getFirmwareVersion())) {
            ;
        }
    }

    // Print out chip version and firmware version
#ifdef SERIALDEBUG
    Serial.printf("Found chip PN5");
    Serial.printf("%x\n", (versionData >> 24) & 0xFF);
    Serial.printf("Firmware ver. ");
    Serial.printf("%d", (versionData >> 16) & 0xFF);
    Serial.printf(".");
    Serial.printf("%d\n", (versionData >> 8) & 0xFF);
#endif

    /// Obtain LUD06 ///
#ifdef SERIALDEBUG
    Serial.printf("Obtaining LUD06 callback\n");
#endif
    callbackLud06 = getLud06Callback(ENV_LNURL);

    /// Tasks ///
    // Task1code is a task to interact with card
    xTaskCreatePinnedToCore(Task1code, "Task1", 10000, NULL, 0, &Task1GetLnurlwCallback, 0);
    // Task2code is a task to generate invoice
    xTaskCreatePinnedToCore(Task2code, "Task2", 10000, NULL, 0, &Task2GetInvoice, 0);
    // Task3code is a task of status LED
    xTaskCreatePinnedToCore(Task3code, "Task3", 10000, NULL, 0, &Task3ThinkingLed, 0);
    // Task4code is a task to adjust amount
    xTaskCreatePinnedToCore(Task4code, "Task4", 10000, NULL, 0, &Task4FixAmount, 0);
    // Task5code is a task of confirmation card read
    xTaskCreatePinnedToCore(Task5code, "Task5", 10000, NULL, 0, &Task5ConfirmationLed, 0);
}

void loop(void) {
#ifdef SERIALDEBUG
    Serial.printf("\nloop running on core %d\n", xPortGetCoreID());
    Serial.printf("Waiting for an ISO14443A Card\n");
#endif
    amount = "1000"; // reset amount
    vTaskResume(Task4FixAmount);

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
#ifdef SERIALDEBUG
        Serial.printf("Found an ISO14443A card\n");
#endif
        vTaskSuspend(Task4FixAmount);

        if (uidLength == 7 && nfcModule.ntag424_isNTAG424()) { // Read data from the card if it is a NTAG424 card
            bytesRead = nfcModule.ntag424_ISOReadFile(data);
            invoiceWait = true;

            if (bytesRead) {
                vTaskResume(Task5ConfirmationLed);

#ifdef TIMEDEBUG
                Serial.printf("Start time\n");
                unsigned long startTime = timeClient.getEpochTime();
#endif

                vTaskResume(Task1GetLnurlwCallback);
                vTaskResume(Task2GetInvoice);

                while (eTaskGetState(Task1GetLnurlwCallback) != eSuspended ||
                       eTaskGetState(Task3ThinkingLed) != eSuspended) { // Wait for the tasks to finish
                    delay(10);
                }

                DynamicJsonDocument doc = claimInvoice(callbackLnurlw, invoice); //*! TODO: return enum
                invoiceWait = false;

#ifdef TIMEDEBUG
                unsigned long endTime = timeClient.getEpochTime();
                Serial.printf("\nTime elapsed: %d seconds\n", endTime - startTime);
#endif

                //*! TODO: AFUERA!!!
                serializeJsonPretty(doc, Serial); // Print the result after  paying the invoice (or try to)
                invoiceStatus = doc["status"].as<String>();
            }
#ifdef SERIALDEBUG
            else {
                Serial.printf("ERROR: Failed to read data\n");
            }
#endif
        }
#ifdef SERIALDEBUG
        else {
            Serial.printf("ERROR: Not a NTAG424 card\n");
            delay(500);
        }
#endif
    }
#ifdef SERIALDEBUG
    else {
        Serial.printf("ERROR: Failed to read card\n");
        delay(500);
    }
#endif

    //*! TODO: modify to enum
    if (invoiceStatus == "OK") {
        delay(250);
        screen.clear();
        screen.drawStr(44, 20, "Paid!");
        screen.sendBuffer();

        int amountSats = amount.toInt() / 1000;
        //*! TODO :convert to function
        do {
            digitalWrite(LED_GREEN, HIGH);
            digitalWrite(BUZZER, HIGH);
            delay(250);
            digitalWrite(LED_GREEN, LOW);
            digitalWrite(BUZZER, LOW);
            delay(250);
        } while (amountSats /= 10);
    } else {
        delay(250);
        screen.clear();
        screen.drawStr(40, 10, "ERROR!");
        screen.drawStr(28, 20, "Try again");
        screen.sendBuffer();
        //*! TODO convert to function
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_RED, HIGH);
        digitalWrite(BUZZER, HIGH);
        delay(1500);
        digitalWrite(LED_RED, LOW);
        digitalWrite(BUZZER, LOW);
    }

    // Check if the WiFi connection is lost
    if (WiFi.status() != WL_CONNECTED) {
#ifdef SERIALDEBUG
        Serial.printf("Connecting to WiFi");
#endif
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
#ifdef SERIALDEBUG
            Serial.printf(".");
#endif
        }
#ifdef SERIALDEBUG
        Serial.printf("\nConnected to the WiFi network\n");
#endif
    }

    delay(500);
}

// Task5code is a task of confirmation card read
void Task5code(void *vpParameters) {
#ifdef SERIALDEBUG
    Serial.printf("Task5 suspend\n");
#endif
    vTaskSuspend(Task5ConfirmationLed); // Suspend the task when it is created
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

        vTaskResume(Task3ThinkingLed);
        vTaskSuspend(Task5ConfirmationLed);
    }
}

// Task1code is a task to interact with card
void Task1code(void *vpParameters) {
#ifdef SERIALDEBUG
    Serial.printf("Task1 suspend\n");
#endif
    vTaskSuspend(Task1GetLnurlwCallback); // Suspend the task when it is created
    for (;;) {
#ifdef SERIALDEBUG
        Serial.printf("Task1 running on core %d\n", xPortGetCoreID());
#endif

#ifdef TIMEDEBUG
        unsigned long startTime = timeClient.getEpochTime();
#endif

        /// Dump the data ///
        data[bytesRead] = 0;
        String lnurlw = (char *)data;
#ifdef SERIALDEBUG
        Serial.printf("LNURLw from card: %s\n", lnurlw.c_str());
#endif

        callbackLnurlw = getLnurlwCallback(lnurlw); // Obtain LNURLw callback

#ifdef TIMEDEBUG
        unsigned long endTime = timeClient.getEpochTime();
        Serial.printf("\nTime elapsed in Task1: %d seconds\n", endTime - startTime);
#endif

        vTaskSuspend(Task1GetLnurlwCallback);
    }
}

// Task2code is a task to generate invoice
void Task2code(void *vpParameters) {
#ifdef SERIALDEBUG
    Serial.printf("Task2 suspend\n");
#endif
    vTaskSuspend(Task2GetInvoice); // Suspend the task when it is created
    for (;;) {
#ifdef SERIALDEBUG
        Serial.printf("Task2 running on core %d\n", xPortGetCoreID());
#endif

#ifdef TIMEDEBUG
        unsigned long startTime = timeClient.getEpochTime();
#endif

        // Serial.printf("Invoice amount: %s\n", amount.c_str());
        invoice = getInvoice(callbackLud06, amount); // Generate invoice

#ifdef TIMEDEBUG
        unsigned long endTime = timeClient.getEpochTime();
        Serial.printf("\nTime elapsed in Task2: %d seconds\n", endTime - startTime);
#endif

        vTaskSuspend(Task2GetInvoice);
    }
}

// Task3code is a task of status LED
void Task3code(void *vpParameters) {
#ifdef SERIALDEBUG
    Serial.printf("Task3 suspend\n");
#endif
    vTaskSuspend(Task3ThinkingLed); // Suspend the task when it is created
    for (;;) {
#ifdef SERIALDEBUG
        Serial.printf("Task3 running on core %d\n", xPortGetCoreID());
#endif
        screen.clear();
        screen.drawStr(32, 10, "Thinking");
        screen.sendBuffer();

        uint8_t delayTime = 250;
        while (invoiceWait || eTaskGetState(Task1GetLnurlwCallback) != eSuspended ||
               eTaskGetState(Task2GetInvoice) != eSuspended) {
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

        vTaskSuspend(Task3ThinkingLed);
    }
}

// Task4code is a task to adjust amount
void Task4code(void *vpParameters) {
    for (;;) {
#ifdef SERIALDEBUG
        Serial.printf("Task4 running on core %d\n", xPortGetCoreID());
#endif
        if (touchRead(TOUCH_PIN_SUB) < 40) {
            if (amount.toInt() <= 1000) {
#ifdef SERIALDEBUG
                Serial.printf("Minimal limit!\n");
#endif
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
#ifdef SERIALDEBUG
            Serial.printf("Amount: %d SATS\n", amountInt);
#endif
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
#ifdef SERIALDEBUG
            Serial.printf("Amount: %d SATS\n", amountInt);
#endif
            delay(350);
        }
    }
}