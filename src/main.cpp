#include "Bitcoin.h"
#include "Hash.h"
#include "env.hpp"
#include "utils/content.hpp"
#include "utils/getPubkeyFromLnurlw.hpp"
#include "utils/nostrEvent.hpp"
#include "utils/req.hpp"
#include <Adafruit_PN532_NTAG424.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <Wire.h>

/// Debug ///
// Comment the following lines to disable debug
#define SERIALDEBUG
#define TIMEDEBUG
// Comment the following lines to disable extensions acordeglly
#define EXTENSIONTIPS

/// Peripherals ///
// LEDs
#define LED_RED 33
#define LED_YELLOW 25
#define LED_GREEN 26
// Buzzer
#define BUZZER 27
// Touch pins
#define TOUCH_PIN_ADD 15
#define TOUCH_PIN_SUB 13
// Screen
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

/// WebSocket ///
String wsMsg;
bool newMsg = false;
Req req;
WebSocketsClient webSocket; // declare instance of websocket

void webSocketEvent(WStype_t type, uint8_t *strload, size_t length);

/// Extensions ///
// Extension Tips
#ifdef EXTENSIONTIPS
TaskHandle_t threadExtensionTips;
void threadExtensionTipsCode(void *vpParameters);
TaskHandle_t task4ExtensionTipsSetAmount;
void task4ExtensionTipsSetAmountCode(void *vpParameters);
#endif

/// Auxiliar functions ///
uint8_t functionThinkingLed(bool displayedTitle, uint8_t delayTime);
void auxFunctionConfirmationLed(void);
void auxFunctionAmountBlinkingLed(int amount);
void auxFunctionErrorLed(void);
uint8_t auxFunctionThinkingLed(bool displayedTitle, uint8_t delayTime);
void startupConfiguration(void);
String readCard(void);
bool handleResponse(String newEventJson);

/// Global variables ///
String callbackLud06;
String callbackLnurlw;
String invoice;
String amount;
PrivateKey privateKey(ENV_SEC_HEX_32);
PublicKey publicKey = privateKey.publicKey();
bool confirmationPaid = false;
String workers[30] = {""};
String eventIdString;
bool eventMsgSend = false;
bool title = false;
uint8_t delayTime = 255;

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

    /// Create req ///
    req.kinds = "20001";
    req.authors = ENV_SERVER_PUB_HEX;
    req.since = timeClient.getEpochTime();
    req.until = "1739302824";

    /// Init WebSocket ///
    webSocket.beginSSL(ENV_RELAY, 443);
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(1000);
    while (!webSocket.isConnected()) {
        Serial.print(".");
        webSocket.loop();
    }
    Serial.printf("Conceted to websocket!\n");

#ifdef TIMEDEBUG
    /// Init NTP ///
    timeClient.begin();
    timeClient.update();
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
    xTaskCreatePinnedToCore(threadExtensionTipsCode, "threadExtensionTips", 10000, NULL, 0, &threadExtensionTips, 0);

    // task4ExtensionTipsSetAmountCode is a task to adjust amount
    xTaskCreatePinnedToCore(task4ExtensionTipsSetAmountCode, "Task4ExtensionTips", 10000, NULL, 0,
                            &task4ExtensionTipsSetAmount, 0);
#endif

    startupConfiguration();

    Serial.printf("Setup finished\n");
}

bool isStart = false;

void loop(void) {
#ifdef EXTENSIONTIPS
    if (!isStart) {
        vTaskResume(task4ExtensionTipsSetAmount);
        vTaskResume(threadExtensionTips);
        isStart = true;
    }
#endif

#ifdef SERIALDEBUG
    Serial.printf("\nloop running on core %d\n", xPortGetCoreID());
#endif

    while (newMsg == false) {
        webSocket.loop();
    }

#ifdef SERIALDEBUG
    DynamicJsonDocument wsMsgDoc(2048);
    DeserializationError error = deserializeJson(wsMsgDoc, wsMsg);
    if (error) {
        Serial.printf("Deserialization wsMsg error: %s\n", error.c_str());
        delay(500);
    }

    Serial.printf("Message from WS:\n");
    serializeJsonPretty(wsMsgDoc, Serial);
#endif

    NostrEvent eventRecived(wsMsg);
    bool eventMsgRecived = eventRecived.compareId(eventIdString);

    if (!eventMsgRecived && eventMsgSend) {
        vTaskSuspend(task4ExtensionTipsSetAmount);
        // delayTime = auxFunctionThinkingLed(title, delayTime);
        // title = true;
        screen.clear();
        screen.drawStr(0, 10, "Waiting response");
        screen.sendBuffer();
        eventMsgSend = false;
    }

    if (wsMsgDoc[0] == "EVENT" && eventMsgRecived) {
        if (handleResponse(wsMsg)) {
            Serial.printf("Is a valid event\n");
            screen.clear();
            screen.drawStr(0, 10, "Payment confirmed");
            screen.sendBuffer();
            // title = false;
            auxFunctionConfirmationLed();
            delay(1000);
        } else {
            Serial.printf("Is a invalid event\n");
            screen.clear();
            screen.drawStr(0, 10, "Payment rejected");
            screen.sendBuffer();
            auxFunctionErrorLed();
            delay(1000);
        }
        eventMsgRecived = false;
#ifdef EXTENSIONTIPS
        vTaskResume(task4ExtensionTipsSetAmount);
        vTaskResume(threadExtensionTips);
#endif
    }

    newMsg = false;

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
    digitalWrite(LED_YELLOW, HIGH);
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
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_RED, HIGH);
    digitalWrite(BUZZER, HIGH);
    delay(1500);
    digitalWrite(LED_RED, LOW);
    digitalWrite(BUZZER, LOW);
    digitalWrite(LED_YELLOW, HIGH);
}

String readCard(void) {
    uint8_t data[256];
    uint8_t bytesRead;
    uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer to store the returned UID
    uint8_t uidLength;                     // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    for (;;) {
#ifdef SERIALDEBUG
        Serial.printf("Waiting for an ISO14443A readCard\n");
#endif
        uint8_t success = nfcModule.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

        if (success) {
#ifdef SERIALDEBUG
            Serial.printf("Found an ISO14443A card\n");
#endif
            if (uidLength == 7 && nfcModule.ntag424_isNTAG424()) { // Read data from the card if it is a NTAG424 card
                bytesRead = nfcModule.ntag424_ISOReadFile(data, sizeof(data));

                if (bytesRead) {
                    auxFunctionConfirmationLed();
                    data[bytesRead] = 0;

                    return (char *)data;
                }
#ifdef SERIALDEBUG
                else {
                    auxFunctionErrorLed();
                    Serial.printf("ERROR: Failed to read data\n");
                }
#endif
            }
#ifdef SERIALDEBUG
            else {
                Serial.printf("ERROR: Not a NTAG424 card\n");
            }
#endif
        }
    }
}

bool handleResponse(String newEventJson) {
    DynamicJsonDocument eventDoc(2000);
    DeserializationError error = deserializeJson(eventDoc, newEventJson);
    if (error) {
        Serial.printf("Deserialization eventJson error (handleResponse func): %s\n", error.c_str());
        delay(500);
    }
    String content = eventDoc.as<JsonArray>()[2].as<JsonObject>()["content"].as<String>();

    // {"success":"true","error":"invalid keys"}
    DynamicJsonDocument contentDoc(512);
    error = deserializeJson(contentDoc, content);
    serializeJsonPretty(contentDoc, Serial);
    if (error) {
        Serial.printf("Deserialization content error: %s\n", error.c_str());
        return false;
    }

    if (contentDoc.as<JsonObject>()["status"].as<String>() == "true") {
        return true;
    } else if (contentDoc.as<JsonObject>()["status"].as<String>() == "false") {
#ifdef SERIALDEBUG
        String erorr = contentDoc.as<JsonObject>()["error"].as<String>();
        Serial.printf("Error: %s\n", erorr.c_str());
#endif
        return false;
    } else {
        return false;
    }
}

/*************************************************************************************************/
/*                                     Extension Tips                                            */
/*************************************************************************************************/

#ifdef EXTENSIONTIPS
/// threadExtensionTipsCode is a task to tips extension ///
void threadExtensionTipsCode(void *vpParameters) {
#ifdef SERIALDEBUG
    Serial.printf("threadExtensionTipsCode suspend\n");
#endif
    vTaskSuspend(threadExtensionTips); // Suspend the task when it is created
    for (;;) {
#ifdef SERIALDEBUG
        Serial.printf("threadExtensionTipsCode running on core %d\n", xPortGetCoreID());
#endif
        amount = "1000"; // reset amount
        vTaskResume(task4ExtensionTipsSetAmount);

        digitalWrite(LED_YELLOW, HIGH);
        screen.clear();
        screen.drawStr(0, 10, "Invoice amount:");
        screen.drawStr(48, 20, ((String)(amount.toInt() / 1000)).c_str());
        screen.drawStr(0, 32, "Tap card to pay");
        screen.sendBuffer();

        String lnurlw = readCard();
        vTaskSuspend(task4ExtensionTipsSetAmount);
        // vTaskResume(task3ExtensionTipsAfterReadCard);

        String tags[2][2] = {{"action", "http"}, {"p", ENV_SERVER_PUB_HEX}};

        Content content(workers, lnurlw, amount);

        NostrEvent newEvent(toHex(publicKey.point, 32), String(timeClient.getEpochTime()), "20001", tags,
                            content.toJson());

        uint8_t messageHash[32];
        sha256(newEvent.toJsonToSign(), messageHash);
        SchnorrSignature signature = privateKey.schnorr_sign(messageHash);
        String eventSignatureString = signature.toString();
        eventIdString = toHex(messageHash, 32);

        // verify signature
        // if (publicKey.schnorr_verify(signature, messageHash)) {
        //     Serial.printf("All good, signature is valid\n");
        // } else {
        //     Serial.printf("Something is wrong! Signature is invalid\n");
        // }

        newEvent.id = eventIdString;
        newEvent.sig = eventSignatureString;

        String eventJsonToSend = "[\"EVENT\"," + newEvent.toJson() + "]";

        // Serial.printf("\n\nEVENT pre sign:\n%s\n\n", newEvent.toJson().c_str());
        // Serial.printf("\n\nEVENT:\n%s\n\n", eventJsonToSend.c_str());

        webSocket.sendTXT(eventJsonToSend.c_str());
        eventMsgSend = true;
        vTaskSuspend(threadExtensionTips);
    }
}

/// task4ExtensionTipsSetAmountCode is a task to adjust amount ///
void task4ExtensionTipsSetAmountCode(void *vpParameters) {
#ifdef SERIALDEBUG
    Serial.printf("task4ExtensionTipsSetAmountCode suspend\n");
#endif
    vTaskSuspend(task4ExtensionTipsSetAmount); // Suspend the task when it is created
    for (;;) {
#ifdef SERIALDEBUG
        // Serial.printf("task4ExtensionTipsSetAmountCode running on core %d\n", xPortGetCoreID());
#endif
        int amountToSubAdd = 100000;
        while (touchRead(TOUCH_PIN_SUB) < 40) {
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
                    amount = amount.toInt() == 21000 ? amount.toInt() - 20000 : amount.toInt() - amountToSubAdd;
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

            // uint8_t count = 0;
            // count++;
            // if (count == 3) {
            //     amountToSubAdd = 1000000;
            // } else if (count == 9) {
            //     amountToSubAdd = 10000000;
            // }
        }
        while (touchRead(TOUCH_PIN_ADD) < 40) {
            if (amount.toInt() == 1000) {
                amount = amount.toInt() + 20000;
            } else {
                amount = amount.toInt() == 21000 ? amount.toInt() + 79000 : amount.toInt() + amountToSubAdd;
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

            // uint8_t count;
            // count++;
            // if (count == 3) {
            //     amountToSubAdd = 1000000;
            // } else if (count == 9) {
            //     amountToSubAdd = 10000000;
            // }
        }
    }
}
#endif // EXTENSIONTIPS

/*************************************************************************************************/
/*                                    Startup Configuration                                      */
/*************************************************************************************************/

void startupConfiguration(void) {
    uint8_t workersCount = 0;
    bool addOther = false;

    screen.clear();
    screen.drawStr(0, 10, "Welcome!");
    screen.drawStr(0, 20, "Tap to add");
    screen.drawStr(0, 30, "worker");
    screen.sendBuffer();

    do {
        String pubkeyAndName[2]; // 0: pubkey, 1: name
        getPubkeyFromLnurlw(readCard(), pubkeyAndName);
        workers[workersCount++] = pubkeyAndName[0];
#ifdef SERIALDEBUG
        Serial.printf("Worker added: %s\n", pubkeyAndName[1].c_str());
#endif
        screen.clear();
        screen.drawStr(0, 10, "Added worker:");
        screen.drawStr(0, 20, pubkeyAndName[1].c_str());
        screen.sendBuffer();
        delay(1000);

#ifdef SERIALDEBUG
        Serial.printf("Do you want add other worker?\n");
#endif
        screen.clear();
        screen.drawStr(0, 10, "Add worker?");
        screen.drawStr(0, 20, "+100 confirm");
        screen.drawStr(0, 30, "-100 reject");
        screen.sendBuffer();

        int touchAdd = 40, touchSub = 40;
        while ((touchAdd = touchRead(TOUCH_PIN_ADD)) > 40 && (touchSub = touchRead(TOUCH_PIN_SUB)) > 40) {
            ;
        }
        if (touchAdd < 40) {
            addOther = true;
            screen.clear();
            screen.drawStr(0, 20, "tap card");
            screen.sendBuffer();
        } else if (touchSub < 40) {
            addOther = false;
        }
    } while (addOther);

    Serial.printf("Startup configuration finished\n");
    Serial.printf("Workers:\n");
    for (int i = 0; i < workersCount; i++) {
        Serial.printf("[%d] %s", i + 1, workers[i].c_str());
        if (i == 0) {
            Serial.printf(" (admin)\n");
        } else {
            Serial.printf("\n");
        }
    }

    return;
}

/*************************************************************************************************/
/*                                       WebSocket                                               */
/*************************************************************************************************/

void webSocketEvent(WStype_t type, uint8_t *strload, size_t length) {
    if (type == WStype_CONNECTED) {
        timeClient.update();
        req.since = timeClient.getEpochTime() - 10;
        Serial.printf("\nsince updated\n");
    }

    switch (type) {
    case WStype_DISCONNECTED:
        Serial.printf("[WS] Disconnected!\n");
        break;
    case WStype_CONNECTED:
        Serial.printf("\n[WS] Connected\n");

        Serial.printf("Sending request: %s\n", req.toJson().c_str());

        webSocket.sendTXT(req.toJson().c_str()); // send message to server when Connected
        break;
    case WStype_TEXT:
        wsMsg = (char *)strload;
        Serial.printf("\n=== Received data from socket ===\n");
        newMsg = true;
        break;
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
        break;
    }
}