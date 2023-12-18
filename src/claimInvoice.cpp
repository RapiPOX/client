#include "claimInvoice.hpp"
#include <HTTPClient.h>

DynamicJsonDocument claimInvoice(String lnurlwCallback, String invoice) {
    HTTPClient client;
    String httppayload;

    // Construct the full URL with the invoice as a query parameter
    String fullUrl = lnurlwCallback + "&pr=" + invoice;

    client.begin(fullUrl);       // Specify request destination
    int httpCode = client.GET(); // get request

    if (httpCode) {
        httppayload = client.getString(); // get response
    } else {
        Serial.printf("[HTTP] GET claim invoice failed (claimInvoice file), error: %s\n",
                      client.errorToString(httpCode).c_str());
    }

    client.end(); // Close connection

    DynamicJsonDocument doc(1024);
    if (httpCode >= 200 && httpCode < 300) { // Check if the request was successful
        deserializeJson(doc, httppayload);
    } else {
        // Handle the error or indicate a failed request
        doc["error"] = "Request failed with HTTP code: " + String(httpCode) + " (calimInvoice file)";
    }

    return doc;
}