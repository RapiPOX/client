#include "lud06Request.hpp"
#include <ArduinoJson.h>
#include <HTTPClient.h>

String generateLud06Url(String lnurl) {
    // Find the position of the @ character
    int atIndex = lnurl.indexOf("@");
    if (atIndex == -1) {
        return ""; // Return an empty string if @ is not found
    }

    // Extract the username and domain parts
    String username = lnurl.substring(0, atIndex);
    String domain = lnurl.substring(atIndex + 1);

    // Construct the new URL
    String url = "https://" + domain + "/.well-known/lnurlp/" + username;

    return url;
}

String getLud06Callback(String lnurl) {
    HTTPClient client;
    String httpPayload;

    String lud06Url = generateLud06Url(lnurl);

    client.begin(lud06Url);      // Specify request destination
    int httpCode = client.GET(); // get request

    if (httpCode >= 200 && httpCode < 300) {
        httpPayload = client.getString(); // get response
    } else {
        Serial.printf("[HTTP] GET LUD06 failed (lud06Request file), error: %s\n",
                      client.errorToString(httpCode).c_str());
    }

    client.end(); // Close connection

    DynamicJsonDocument doc(1024);
    if (httpCode >= 200 && httpCode < 300) { // Check if the request was successful
        deserializeJson(doc, httpPayload);
    }

    return doc.as<JsonObject>()["callback"].as<String>();
}