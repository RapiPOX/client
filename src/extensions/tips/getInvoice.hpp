#ifndef GENERATEINVOICE_HPP
#define GENERATEINVOICE_HPPP

#include <Arduino.h>

/// @brief Get invoice from LUD06
/// @param callbackLud06 callback url from LUD06
/// @return Generated invoice
String getInvoice(String callbackLud06, String amount);

#endif