#include <ArduinoJson.h>
#include <HTTPClient.h>

void getPubkeyFromLnurlw(String lnurlw, String pubkeyAndName[]) {
    HTTPClient client;
    String httpPayload;

    lnurlw.replace("lnurlw://", "https://");

    client.begin(lnurlw); // Specify request destination

    client.addHeader("x-lawallet-action", "info");
    client.addHeader("x-lawallet-param", "federationId=lawallet.ar");

    int httpCode = client.GET(); // get request

    if (httpCode >= 200 && httpCode < 300) {
        httpPayload = client.getString(); // get response
    } else {
        Serial.printf("[HTTP] GET pubkey failed (getPubkeyFromLnurlw file), error: %s\n",
                      client.errorToString(httpCode).c_str());
    }

    client.end(); // Close connection

    DynamicJsonDocument doc(5000);
    if (httpCode >= 200 && httpCode < 300) { // Check if the request was successful
        deserializeJson(doc, httpPayload);
    }

    pubkeyAndName[0] = doc.as<JsonObject>()["info"]
                           .as<JsonObject>()["holder"]
                           .as<JsonObject>()["ok"]
                           .as<JsonObject>()["pubKey"]
                           .as<String>();

    pubkeyAndName[1] = doc.as<JsonObject>()["info"]
                           .as<JsonObject>()["identity"]
                           .as<JsonObject>()["ok"]
                           .as<JsonObject>()["name"]
                           .as<String>();
}