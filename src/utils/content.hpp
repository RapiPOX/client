#ifndef CONTENT_HPP
#define CONTENT_HPP

#include <Arduino.h>

class Content {
  public:
    String workers[30] = {""};
    String lnurlw;
    String amount;

    Content() {
        lnurlw = "";
        amount = "";
    }

    Content(String workers[], String lnurlw, String amount) {
        for (int i = 0; workers[i] != ""; ++i) {
            this->workers[i] = workers[i];
        }
        this->lnurlw = lnurlw;
        this->amount = amount;
    }

    String toJson(void) {
        String json = "{\"pubkeys\":[";
        for (int i = 0; workers[i] != ""; ++i) {
            json += "\"" + workers[i] + "\"";
            if (workers[i + 1] != "") {
                json += ",";
            }
        }
        json += "],";
        json += "\"lnurlw\":\"" + lnurlw + "\",";
        json += "\"amount\":" + amount + "}";

        Serial.println(json);

        return json;
    }
};

#endif