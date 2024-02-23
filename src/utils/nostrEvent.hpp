#ifndef NOSTREVENT_HPP
#define NOSTREVENT_HPP

#include "escapeJson.hpp"
#include <Arduino.h>
#include <ArduinoJson.h>

class NostrEvent {
  public:
    String id;
    String pubkey;
    String created_at;
    String kind;
    String tags[2][2];
    String content;
    String sig;

    NostrEvent() {
        id = "";
        pubkey = "";
        created_at = "0";
        kind = "0";
        tags[0][0] = "";
        tags[0][1] = "";
        tags[1][0] = "";
        tags[1][1] = "";
        content = "";
        sig = "";
    }

    NostrEvent(String pubkey, String created_at, String kind, String tags[2][2], String content) {
        this->pubkey = pubkey;
        this->created_at = created_at;
        this->kind = kind;
        this->tags[0][0] = tags[0][0];
        this->tags[0][1] = tags[0][1];
        this->tags[1][0] = tags[1][0];
        this->tags[1][1] = tags[1][1];
        this->content = escapeJson(content);
    }

    NostrEvent(String json) {
        if (strstr(json.c_str(), "EVENT") == NULL) {
            Serial.println("NostrEvent constructor: json is not an event");
            return;
        }
        DynamicJsonDocument docPre(2048);
        DeserializationError error = deserializeJson(docPre, json);
        if (error) {
            Serial.printf("Deserialization error (NostrEvent constructor): %s\n", error.c_str());
            return;
        }
        JsonObject doc = docPre.as<JsonArray>()[2].as<JsonObject>();

        id = doc["id"].as<String>();
        pubkey = doc["pubkey"].as<String>();
        created_at = doc["created_at"].as<String>();
        kind = doc["kind"].as<String>();
        content = doc["content"].as<String>();
        sig = doc["sig"].as<String>();

        JsonArray tagsArray = doc["tags"];
        for (size_t i = 0; i < tagsArray.size(); ++i) {
            JsonArray innerArray = tagsArray[i];
            for (size_t j = 0; j < innerArray.size(); ++j) {
                tags[i][j] = innerArray[j].as<String>();
            }
        }
    }

    String toJson(void) {
        String json = "{";
        json += "\"id\":\"" + id + "\",";
        json += "\"pubkey\":\"" + pubkey + "\",";
        json += "\"created_at\":" + created_at + ",";
        json += "\"kind\":" + kind + ",";
        json += "\"tags\":[[\"" + tags[0][0] + "\",\"" + tags[0][1] + "\"],[\"" + tags[1][0] + "\",\"" + tags[1][1] +
                "\"]],";
        json += "\"content\":\"" + content + "\",";
        json += "\"sig\":\"" + sig + "\"";
        json += "}";

        return json;
    }

    String toJsonToSign(void) {
        String json = "[";
        json += "0,";
        json += "\"" + pubkey + "\",";
        json += created_at + ",";
        json += kind + ",";
        json += "[[\"" + tags[0][0] + "\",\"" + tags[0][1] + "\"],[\"" + tags[1][0] + "\",\"" + tags[1][1] + "\"]],";
        json += "\"" + content + "\"";
        json += "]";

        return json;
    }

    String getContentUrl() {
        return content.substring(content.indexOf("https://"), content.indexOf("\"}")); // get only url
    }

    String print() {
        String json = "{\n";
        json += "  \"id\": \"" + id + "\",\n";
        json += "  \"pubkey\": \"" + pubkey + "\",\n";
        json += "  \"created_at\": " + created_at + ",\n";
        json += "  \"kind\": " + kind + ",\n";
        json += "  \"tags\": [[\"" + tags[0][0] + "\",\"" + tags[0][1] + "\"],[\"" + tags[1][0] + "\",\"" + tags[1][1] +
                "\"]],\n";
        json += "  \"content\": \"" + content + "\",\n";
        json += "  \"sig\": \"" + sig + "\"\n";
        json += "}";

        return json;
    }

    bool compareId(String eventIdCreated) {
        for (int i = 0; i < 2; i++) {
            if (tags[i][0] == "e") {
                if (tags[i][1] == eventIdCreated) {
                    return true;
                }
            }
        }

        return false;
    }
};

#endif