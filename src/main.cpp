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
// Comment the following lines to disable extensions acordeglly
#define EXTENSIONTIPS

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

/// Extensions ///
// Extension Tips
#ifdef EXTENSIONTIPS
TaskHandle_t threadExtensionTips;
void threadExtensionTipsCode(void *vpParameters);

TaskHandle_t task1ExtensionTipsGetLnurlwCallback;
void task1ExtensionTipsGetLnurlwCallbackCode(void *vpParameters);
TaskHandle_t task2ExtensionTipsGetInvoice;
void task2ExtensionTipsGetInvoiceCode(void *vpParameters);
TaskHandle_t task3ExtensionTipsAfterReadCard;
void task3ExtensionTipsAfterReadCardCode(void *vpParameters);
TaskHandle_t task4ExtensionTipsSetAmount;
void task4ExtensionTipsSetAmountCode(void *vpParameters);
#endif

// Task parameters
// struct Task1Parameters {
//     String lnurlw;
// };

/// Auxiliar functions ///
uint8_t functionThinkingLed(bool displayedTitle, uint8_t delayTime);
void auxFunctionConfirmationLed(void);
void auxFunctionAmountBlinkingLed(int amount);
void auxFunctionErrorLed(void);

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

#ifdef EXTENSIONTIPS
    /// Extension Tips ///
    // threadExtensionTipsCode is a task to tips extension
    xTaskCreatePinnedToCore(threadExtensionTipsCode, "threadExtensionTips", 10000, NULL, 0, &threadExtensionTips, 1);

    // task1ExtensionTipsGetLnurlw is a task to interact with card
    xTaskCreatePinnedToCore(task1ExtensionTipsGetLnurlwCallbackCode, "Task1ExtensionTips", 10000, NULL, 0,
                            &task1ExtensionTipsGetLnurlwCallback, 0);
    // task2ExtensionTipsGetInvoiceCode is a task to generate invoice
    xTaskCreatePinnedToCore(task2ExtensionTipsGetInvoiceCode, "Task2ExtensionTips", 10000, NULL, 0,
                            &task2ExtensionTipsGetInvoice, 0);
    // task3ExtensionTipsThinkingLedCode is a task of thinking LED
    xTaskCreatePinnedToCore(task3ExtensionTipsAfterReadCardCode, "Task3ExtensionTips", 10000, NULL, 0,
                            &task3ExtensionTipsAfterReadCard, 0);
    // task4ExtensionTipsSetAmountCode is a task to adjust amount
    xTaskCreatePinnedToCore(task4ExtensionTipsSetAmountCode, "Task4ExtensionTips", 10000, NULL, 0,
                            &task4ExtensionTipsSetAmount, 0);
#endif
}

void loop(void) {
#ifdef SERIALDEBUG
    Serial.printf("\nloop running on core %d\n", xPortGetCoreID());
#endif

    // #ifdef EXTENSIONTIPS
    //     vTaskResume(threadExtensionTips);
    // #endif

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

/*************************************************************************************************/
/*                                   Auxiliar functions                                          */
/*************************************************************************************************/

uint8_t auxFunctionThinkingLed(bool displayedTitle, uint8_t delayTime) {
    if (!displayedTitle) {
        screen.clear();
        screen.drawStr(32, 10, "Thinking");
        screen.sendBuffer();
    }

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

    return delayTime > 50 ? delayTime - 10 : 50;
}

void auxFunctionConfirmationLed(void) {
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
}

void auxFunctionAmountBlinkingLed(int amount) {
    do {
        digitalWrite(LED_GREEN, HIGH);
        digitalWrite(BUZZER, HIGH);
        delay(250);
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(BUZZER, LOW);
        delay(250);
    } while (amount /= 10);
}

void auxFunctionErrorLed(void) {
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);
    digitalWrite(BUZZER, HIGH);
    delay(1500);
    digitalWrite(LED_RED, LOW);
    digitalWrite(BUZZER, LOW);
}

/*************************************************************************************************/
/*                                     Extension Tips                                            */
/*************************************************************************************************/

#ifdef EXTENSIONTIPS
/// threadExtensionTipsCode is a task to tips extension ///
void threadExtensionTipsCode(void *vpParameters) {
    // Obtain LUD06
#ifdef SERIALDEBUG
    Serial.printf("Obtaining LUD06 callback\n");
#endif
    callbackLud06 = getLud06Callback(ENV_LNURL);

#ifdef SERIALDEBUG
    Serial.printf("threadExtensionTipsCode suspend\n");
#endif
    vTaskSuspend(threadExtensionTips); // Suspend the task when it is created
    for (;;) {
#ifdef SERIALDEBUG
        Serial.printf("threadExtensionTipsCode running on core %d\n", xPortGetCoreID());
        Serial.printf("Waiting for an ISO14443A Card\n");
#endif
        amount = "1000"; // reset amount
        vTaskResume(task4ExtensionTipsSetAmount);

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
            vTaskSuspend(task4ExtensionTipsSetAmount);

            if (uidLength == 7 && nfcModule.ntag424_isNTAG424()) { // Read data from the card if it is a NTAG424 card
                bytesRead = nfcModule.ntag424_ISOReadFile(data);
                invoiceWait = true;

                if (bytesRead) {
                    vTaskResume(task3ExtensionTipsAfterReadCard);

#ifdef TIMEDEBUG
                    Serial.printf("Start time\n");
                    unsigned long startTime = timeClient.getEpochTime();
#endif

                    vTaskResume(task1ExtensionTipsGetLnurlwCallback);
                    vTaskResume(task2ExtensionTipsGetInvoice);

                    while (eTaskGetState(task1ExtensionTipsGetLnurlwCallbackCode) != eSuspended ||
                           eTaskGetState(task3ExtensionTipsAfterReadCardCode) !=
                               eSuspended) { // Wait for the tasks to finish
                        delay(10);
                    }

                    DynamicJsonDocument doc = claimInvoice(callbackLnurlw, invoice); //*! TODO: return enum
                    invoiceWait = false;

#ifdef TIMEDEBUG
                    unsigned long endTime = timeClient.getEpochTime();
                    Serial.printf("\nTime elapsed: %d seconds\n", endTime - startTime);
#endif

                    ///*! TODO: AFUERA!!! ///
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
            auxFunctionAmountBlinkingLed(amountSats);
        } else {
            delay(250);
            screen.clear();
            screen.drawStr(40, 10, "ERROR!");
            screen.drawStr(28, 20, "Try again");
            screen.sendBuffer();
            auxFunctionErrorLed();
        }
    }
}

/// task1ExtensionTipsGetLnurlw is a task to interact with card ///
void task1ExtensionTipsGetLnurlw(void *vpParameters) {
#ifdef SERIALDEBUG
    Serial.printf("task1ExtensionTipsGetLnurlwCallbackCode suspend\n");
#endif
    vTaskSuspend(task1ExtensionTipsGetLnurlwCallback); // Suspend the task when it is created
    for (;;) {
#ifdef SERIALDEBUG
        Serial.printf("task1ExtensionTipsGetLnurlwCallbackCode running on core %d\n", xPortGetCoreID());
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
        Serial.printf("\nTime elapsed in task1ExtensionTipsGetLnurlwCallbackCode: %d seconds\n", endTime - startTime);
#endif

        vTaskSuspend(task1ExtensionTipsGetLnurlwCallback);
    }
}

/// task2ExtensionTipsGetInvoiceCode is a task to generate invoice ///
void task2ExtensionTipsGetInvoiceCode(void *vpParameters) {
#ifdef SERIALDEBUG
    Serial.printf("task2ExtensionTipsGetInvoiceCode suspend\n");
#endif
    vTaskSuspend(task2ExtensionTipsGetInvoice); // Suspend the task when it is created
    for (;;) {
#ifdef SERIALDEBUG
        Serial.printf("task2ExtensionTipsGetInvoiceCode running on core %d\n", xPortGetCoreID());
#endif

#ifdef TIMEDEBUG
        unsigned long startTime = timeClient.getEpochTime();
#endif

#ifdef SERIALDEBUG
        Serial.printf("Invoice amount: %s\n", amount.c_str());
#endif

        invoice = getInvoice(callbackLud06, amount); // Generate invoice

#ifdef TIMEDEBUG
        unsigned long endTime = timeClient.getEpochTime();
        Serial.printf("\nTime elapsed in Task2: %d seconds\n", endTime - startTime);
#endif

        vTaskSuspend(task2ExtensionTipsGetInvoice);
    }
}

/// task3ExtensionTipsAfterReadCardCode is a task to confirm and thinking led after read card ///
void task3ExtensionTipsAfterReadCardCode(void *vpParameters) {
#ifdef SERIALDEBUG
    Serial.printf("task3ExtensionTipsAfterReadCardCode suspend\n");
#endif
    vTaskSuspend(task3ExtensionTipsAfterReadCard); // Suspend the task when it is created
    for (;;) {
#ifdef SERIALDEBUG
        Serial.printf("task3ExtensionTipsAfterReadCardCode running on core %d\n", xPortGetCoreID());
#endif
        auxFunctionConfirmationLed();
        screen.clear();
        screen.drawStr(24, 20, "Read card!");
        screen.sendBuffer();
        delay(500);

        uint8_t delayTime = 250;
        bool displayedTitle = false;
        while (invoiceWait || eTaskGetState(task1ExtensionTipsGetLnurlwCallbackCode) != eSuspended ||
               eTaskGetState(task2ExtensionTipsGetInvoiceCode) != eSuspended) {
            delayTime = auxFunctionThinkingLed(displayedTitle, delayTime);
            displayedTitle = true;
        }

        vTaskSuspend(task3ExtensionTipsAfterReadCard);
    }
}

/// task4ExtensionTipsSetAmountCode is a task to adjust amount ///
void task4ExtensionTipsSetAmountCode(void *vpParameters) {
    for (;;) {
#ifdef SERIALDEBUG
        Serial.printf("task4ExtensionTipsSetAmountCode running on core %d\n", xPortGetCoreID());
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
#endif // EXTENSIONTIPS