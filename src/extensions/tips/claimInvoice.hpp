#ifndef CLAIMINVOICE_HPP
#define CLAIMINVOICE_HPP

#include <ArduinoJson.h>

/// @brief Claim invoice from the card to LNURL
/// @param lnurlwCallback LNURLw callback form the card
/// @param invoice Invoice to claim
/// @return JSON response from the invoice claim
DynamicJsonDocument claimInvoice(String lnurlwCallback, String invoice);

#endif