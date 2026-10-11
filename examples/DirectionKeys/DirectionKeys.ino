// DirectionKeys - uses a thumb stick like four arrow keys and prints each key press.
//
// A direction fires once when the stick is pushed, and repeats while it is held. Thresholds
// and timing are set with DirectionConfig.

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

DirectionConfig keys()
{
  DirectionConfig c;
  c.repeatDelayMs    = 800;   // start repeating sooner than the default 1.5 s
  c.repeatIntervalMs = 200;
  return c;
}

Joystick<2> stick({ cAxisX, cAxisY }, keys());

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

  if (stick.isUp())    Serial.println("up");
  if (stick.isDown())  Serial.println("down");
  if (stick.isLeft())  Serial.println("left");
  if (stick.isRight()) Serial.println("right");

  delay(20);
}
