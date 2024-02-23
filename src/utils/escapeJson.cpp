#include "escapeJson.hpp"

String escapeJson(const String &input) {
    String output;
    for (size_t i = 0; i < input.length(); ++i) {
        char c = input.charAt(i);
        switch (c) {
        case '"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\b':
            output += "\\b";
            break;
        case '\f':
            output += "\\f";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (c < ' ' || c >= 127) {
                // Control characters and non-ASCII characters are escaped using \uXXXX notation
                output += "\\u";
                output += String((uint8_t)c >> 4, HEX);
                output += String((uint8_t)c & 0x0F, HEX);
            } else {
                output += c;
            }
        }
    }
    return output;
}