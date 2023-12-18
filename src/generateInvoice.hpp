#ifndef GENERATEINVOICE_HPP
#define GENERATEINVOICE_HPPP

#include <Arduino.h>

/// @brief Generate invoice from LUD06
/// @param callbackLud06 callback url from LUD06
/// @return Generated invoice
String generateInvoice(String callbackLud06);

#endif