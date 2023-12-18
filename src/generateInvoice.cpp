#include "generateInvoice.hpp"
#include <ArduinoJson.h>
#include <HTTPClient.h>

String generateInvoice(String callbackLud06) {
    HTTPClient client;
    String httpPayload;

    String invoice;

    client.begin(callbackLud06 + "?amount=" + "1000"); // Specify request destination with amount pre-established.
    int httpCode = client.GET();                       // get request

    if (httpCode) {
        httpPayload = client.getString(); // get response
    } else {
        Serial.printf("[HTTP] GET invoice failed (generateInvoice file), error: %s\n",
                      client.errorToString(httpCode).c_str());
    }

    client.end();

    DynamicJsonDocument doc(1024);
    if (httpCode == 200) { // Check if the request was successful
        deserializeJson(doc, httpPayload);
    }

    return doc.as<JsonObject>()["pr"].as<String>();
}