#include "getInvoice.hpp"
#include <ArduinoJson.h>
#include <HTTPClient.h>

String getInvoice(String callbackLud06, String amount) {
    HTTPClient client;
    String httpPayload;

    client.begin(callbackLud06 + "?amount=" + amount); // Specify request destination with amount pre-established.

    int httpCode = client.GET(); // get request

    if (httpCode >= 200 && httpCode < 300) {
        httpPayload = client.getString(); // get response
    } else {
        Serial.printf("[HTTP] GET invoice failed (generateInvoice file), error: %s\n",
                      client.errorToString(httpCode).c_str());
    }

    client.end();

    DynamicJsonDocument doc(1024);
    if (httpCode >= 200 && httpCode < 300) { // Check if the request was successful
        deserializeJson(doc, httpPayload);
    }

    return doc.as<JsonObject>()["pr"].as<String>();
}