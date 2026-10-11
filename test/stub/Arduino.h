// Minimal stand-in for the Arduino core, so the library and its examples can be built and
// run on a PC. Only what JoystickUtilities and its examples use.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

// The fake ADC: tests and examples set the raw value of each pin here.
extern int theAnalogValues[64];
int analogRead(uint8_t pin);

unsigned long millis();
void delay(unsigned long ms);

class HostSerial
{
  public:
    void begin(unsigned long) {}
    int  available() { return 1; }
    int  read()      { return '\n'; }   // every prompt is answered at once

    size_t print(const char* s)        { return static_cast<size_t>(fputs(s, stdout) >= 0 ? std::string(s).size() : 0); }
    size_t print(int value)            { return static_cast<size_t>(printf("%d", value)); }
    size_t print(unsigned int value)   { return static_cast<size_t>(printf("%u", value)); }
    size_t print(long value)           { return static_cast<size_t>(printf("%ld", value)); }
    size_t print(unsigned long value)  { return static_cast<size_t>(printf("%lu", value)); }
    size_t println()                   { return print("\n"); }
    template<typename T>
    size_t println(const T& value)     { size_t n = print(value); return n + println(); }
};

extern HostSerial Serial;
