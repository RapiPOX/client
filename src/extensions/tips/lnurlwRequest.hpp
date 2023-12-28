#ifndef LNURLWREQUEST_HPP
#define LNURLWREQUEST_HPP

#include <Arduino.h>

/// @brief Get callback from LNURLw from the card
/// @param lnurl lnurl from the card
/// @return callback url
String getLnurlwCallback(String lnurlw);

#endif