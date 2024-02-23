#ifndef REQ_HPP
#define REQ_HPP

#include <Arduino.h>

class Req {
  public:
    String kinds;
    String authors;
    String since;
    String until;

    Req(String _kinds = "", String _authors = "", String _since = "", String _until = "") {
        kinds = _kinds;
        authors = _authors;
        since = _since;
        until = _until;
    }
    String toJson() {
        String json = "[\"REQ\",\"query:data\",{";
        json += "\"kinds\":[" + kinds + "],";
        json += "\"authors\":[\"" + authors + "\"],";
        json += "\"since\":" + since + ",";
        json += "\"until\":" + until;
        json += "}]";

        return json;
    };
};

#endif