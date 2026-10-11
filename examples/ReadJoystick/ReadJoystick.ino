// ReadJoystick - prints the position of a two-axis thumb stick ten times a second.
//
// Wiring (HW_GENERIC): the stick's X and Y wipers to GPIO 34 and 35, its supply to 3.3 V.
// Uses the calibration stored by SerialCalibration; without one each axis uses the full ADC range.

#include <JoystickUtilities.h>
using namespace joystickutilities;

// ---- Hardware: pick one -------------------------------------------------------------------
// HW_GENERIC fits any ESP32 with a stick on GPIO 34 (X) and 35 (Y). The others are the devices
// this library was extracted from (see "Origin" in the README).
#define HW_GENERIC
// #define HW_TRANSMITTER             // forgevolt Transmitter, left stick
// #define HW_TINTINROCKETSHOOTER     // forgevolt VectorArcade console
// #define HW_LITTLEGAMECONSOLE       // forgevolt VectorArcade console

#if defined(HW_TRANSMITTER)
const AnalogAxis cAxisX(36), cAxisY(39);
#elif defined(HW_TINTINROCKETSHOOTER)
const AnalogAxis cAxisX(32, true), cAxisY(37);
#elif defined(HW_LITTLEGAMECONSOLE)
const AnalogAxis cAxisX(37, true), cAxisY(38);
#else
const AnalogAxis cAxisX(34), cAxisY(35, true);   // Y inverted: pushing the stick away reads positive
#endif
// ------------------------------------------------------------------------------------------

Joystick<2> stick({ cAxisX, cAxisY });

void setup()
{
  Serial.begin(115200);
  delay(1000);   // gives the Serial Monitor time to connect, so the first lines are not missed
  stick.begin();

#if defined(ESP32)
  // The calibration SerialCalibration stored, if there is one
  if (loadCalibration(stick, "Joystick"))
    Serial.println("Stored calibration loaded.");
  else
    Serial.println("No stored calibration - full ADC range used. Run SerialCalibration first.");
#endif
}

void loop()
{
  stick.update();

  Serial.print("x=");
  Serial.print(stick.getX());
  Serial.print("  y=");
  Serial.println(stick.getY());

  delay(100);
}
