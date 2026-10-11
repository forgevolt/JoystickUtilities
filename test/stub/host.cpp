// The host side of the stubs: fake ADC, clock, Serial and Preferences storage.
#include "Arduino.h"
#include "Preferences.h"

int theAnalogValues[64] = {};
int analogRead(uint8_t pin) { return theAnalogValues[pin % 64]; }

namespace { unsigned long theMillis = 0; }
unsigned long millis()       { return theMillis; }
void delay(unsigned long ms) { theMillis += ms; }

HostSerial Serial;

std::map<std::string, Preferences::Namespace>& Preferences::storage()
{
  static std::map<std::string, Namespace> theStorage;
  return theStorage;
}
