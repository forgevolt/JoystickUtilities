// SerialCalibration - calibrates a two-axis thumb stick through the Serial Monitor.
//
// The library measures; this sketch decides the procedure. Here each step ends when you press
// Enter - a sketch with a display might use a countdown and a button instead, with the same
// building blocks.
//
// Wiring: as in ReadJoystick. On an ESP32 the calibration is stored in flash (Preferences)
// and loaded again at the next start. Set the Serial Monitor to send a newline.

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

const char* const cNameSpace = "Joystick";   // one namespace per stick

// Keeps sampling the stick until Enter arrives
void waitForEnter()
{
  while (true)
  {
    if (Serial.available() > 0 && Serial.read() == '\n')
      return;
    delay(5);
  }
}

void printResult(const CalibrationResult<2>& result)
{
  for (size_t a = 0; a < 2; a++)
  {
    Serial.print(a == 0 ? "X: " : "Y: ");
    Serial.println(result.accepted[a] ? "ok" : "not moved - full range kept");
  }
}

const int cReleases = 3;   // rest positions averaged for the center

void calibrate()
{
  // 1. Center: a sprung stick rests at a slightly different spot depending on the direction it
  //    was released from, so the center is averaged over several releases
  CenterSampler<2> center;
  for (int r = 1; r <= cReleases; r++)
  {
    Serial.print("Center ");
    Serial.print(r);
    Serial.print(" of ");
    Serial.print(cReleases);
    Serial.println(": push the stick in any direction, let it go, then press Enter.");
    waitForEnter();
    for (int i = 0; i < 100; i++)
    {
      center.add(stick);
      delay(5);
    }
  }

  // 2. Range: recorded until Enter
  Serial.println("Move the stick to all edges, then press Enter.");
  RangeTracker<2> range(center.center());
  while (true)
  {
    range.add(stick);
    if (Serial.available() > 0 && Serial.read() == '\n')
      break;
    delay(5);
  }

  // 3. Apply and report
  printResult(stick.setCalibration(range.calibration()));

#if defined(ESP32)
  saveCalibration(stick, cNameSpace);
  Serial.println("Calibration stored.");
#endif
}

void setup()
{
  Serial.begin(115200);
  delay(1000);   // gives the Serial Monitor time to connect, so the first prompt is not missed
  stick.begin();

#if defined(ESP32)
  CalibrationResult<2> stored;
  if (loadCalibration(stick, cNameSpace, &stored))
  {
    Serial.println("Stored calibration loaded:");
    printResult(stored);
  }
#endif

  calibrate();
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
