#include "lnurlwRequest.hpp"
#include <ArduinoJson.h>
#include <HTTPClient.h>

String getLnurlwCallback(String lnurlw) {
    HTTPClient client;
    String httpPayload;

    lnurlw.replace("lnurlw://", "https://");

    client.begin(lnurlw);        // Specify request destination
    int httpCode = client.GET(); // get request

    if (httpCode >= 200 && httpCode < 300) {
        httpPayload = client.getString(); // get response
    } else {
        Serial.printf("[HTTP] GET lnurlw failed (lnurlwRequest file), error: %s\n",
                      client.errorToString(httpCode).c_str());
    }

    client.end(); // Close connection

    DynamicJsonDocument doc(1024);
    if (httpCode >= 200 && httpCode < 300) { // Check if the request was successful
        deserializeJson(doc, httpPayload);
    }

    return doc.as<JsonObject>()["callback"].as<String>() + "?k1=" + doc.as<JsonObject>()["k1"].as<String>();
}