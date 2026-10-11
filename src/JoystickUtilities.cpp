// JoystickUtilities - reading and calibrating physical joysticks on Arduino
// https://github.com/forgevolt/JoystickUtilities
//
// Copyright (c) 2026 Christoph Streit. MIT license, see LICENSE.

#include "JoystickUtilities.h"

#if defined(ESP32)
  #include <Preferences.h>
#endif

namespace joystickutilities
{

namespace
{

// Linear map with float precision; the ranges passed in are never empty (see isUsable()).
float mapRange(float x, float fromLow, float fromHigh, float toLow, float toHigh)
{
  return (x - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow;
}

} // namespace

// ---- AnalogAxis -------------------------------------------------------------------------

AnalogAxis::AnalogAxis(uint8_t pin, bool inverted, const AnalogConfig& config, ReadFunction readFunction)
: myPin(pin), myInverted(inverted), myConfig(config), myRead(readFunction)
{
  if (myConfig.samples < 1)
    myConfig.samples = 1;
  if (myConfig.deadzone < 0)
    myConfig.deadzone = 0;

  resetCalibration();
}

// ----------------------------------------------------------------------------------------
void AnalogAxis::begin()
{
}

// ----------------------------------------------------------------------------------------
uint16_t AnalogAxis::readRaw() const
{
  long sum = 0;
  for (int i = 0; i < myConfig.samples; i++)
    sum += (myRead != nullptr) ? myRead(myPin) : analogRead(myPin);

  long value = sum / myConfig.samples;
  if (value < 0)               value = 0;
  if (value > myConfig.adcMax) value = myConfig.adcMax;

  if (myInverted)
    value = myConfig.adcMax - value;

  return static_cast<uint16_t>(value);
}

// ----------------------------------------------------------------------------------------
int AnalogAxis::read() const
{
  const AxisCalibration& c = myCalibration;
  const int dz = myConfig.deadzone;
  int value = readRaw();

  if (value >= c.center - dz && value <= c.center + dz)
    return 0;

  // Beyond the calibrated ends counts as full deflection
  if (value > c.max) value = c.max;
  if (value < c.min) value = c.min;

  const float mapped = (value > c.center)
                     ? mapRange(value, c.center + dz, c.max, 0, cMAX)
                     : mapRange(value, c.min, c.center - dz, cMIN, 0);

  // The clamp keeps the documented range whatever float rounding does at the ends
  int result = static_cast<int>(mapped);
  if (result > cMAX) result = cMAX;
  if (result < cMIN) result = cMIN;
  return result;
}

// ----------------------------------------------------------------------------------------
bool AnalogAxis::isUsable(const AxisCalibration& calibration) const
{
  const int dz = myConfig.deadzone;
  return calibration.min + 2*dz < calibration.center && calibration.center + 2*dz < calibration.max;
}

// ----------------------------------------------------------------------------------------
bool AnalogAxis::setCalibration(const AxisCalibration& calibration)
{
  if (isUsable(calibration))
  {
    myCalibration = calibration;
    return true;
  }

  resetCalibration();
  return false;
}

// ----------------------------------------------------------------------------------------
void AnalogAxis::resetCalibration()
{
  myCalibration = { 0, myConfig.adcMax / 2, myConfig.adcMax };
}

// ---- DirectionKeys ----------------------------------------------------------------------

DirectionKeys::DirectionKeys(const DirectionConfig& config)
: myConfig(config)
{}

// ----------------------------------------------------------------------------------------
void DirectionKeys::update(int x, int y, unsigned long nowMs)
{
  myUp    = step( y, myUpState,    nowMs);
  myDown  = step(-y, myDownState,  nowMs);
  myRight = step( x, myRightState, nowMs);
  myLeft  = step(-x, myLeftState,  nowMs);
}

// ----------------------------------------------------------------------------------------
// 'value' is signed so that a positive value means "this direction": down and left pass the
// negated axis. Time differences are unsigned, so millis() wrapping around is harmless.
bool DirectionKeys::step(int value, State& state, unsigned long nowMs)
{
  // Below the off threshold: released
  if (value < myConfig.offThreshold)
  {
    state.active    = false;
    state.repeating = false;
    return false;
  }

  // Into the direction: one key press
  if (state.active == false && value > myConfig.onThreshold)
  {
    state.active = true;
    state.since  = nowMs;
    return true;
  }

  // Held long enough: the first repeat
  if (state.active && state.repeating == false && nowMs - state.since > myConfig.repeatDelayMs)
  {
    state.repeating  = true;
    state.lastRepeat = nowMs;
    return true;
  }

  // Repeating: one press per interval
  if (state.active && state.repeating && nowMs - state.lastRepeat > myConfig.repeatIntervalMs)
  {
    state.lastRepeat = nowMs;
    return true;
  }

  return false;
}

// ---- Storing calibrations ---------------------------------------------------------------

#if defined(ESP32)

namespace
{

constexpr char cAxisNames[] = "XYZUVW";

// "min" + "X" etc. Fits the 15-character key limit of Preferences.
void makeKey(char* buffer, size_t size, const char* field, size_t axis)
{
  snprintf(buffer, size, "%s%c", field, cAxisNames[axis]);
}

} // namespace

namespace detail
{

bool saveCalibrations(const char* nameSpace, const AxisCalibration* calibration, size_t count)
{
  Preferences p;
  if (p.begin(nameSpace, false) == false)
    return false;

  char key[16];
  for (size_t i = 0; i < count && i < sizeof(cAxisNames) - 1; i++)
  {
    makeKey(key, sizeof(key), "min", i);     p.putInt(key, calibration[i].min);
    makeKey(key, sizeof(key), "center", i);  p.putInt(key, calibration[i].center);
    makeKey(key, sizeof(key), "max", i);     p.putInt(key, calibration[i].max);
  }

  p.end();
  return true;
}

// ----------------------------------------------------------------------------------------
bool loadCalibrations(const char* nameSpace, AxisCalibration* calibration, size_t count)
{
  // Read-only: opening a namespace that does not exist fails instead of creating it
  Preferences p;
  if (p.begin(nameSpace, true) == false)
    return false;

  char key[16];
  for (size_t i = 0; i < count && i < sizeof(cAxisNames) - 1; i++)
  {
    makeKey(key, sizeof(key), "min", i);     calibration[i].min    = p.getInt(key, calibration[i].min);
    makeKey(key, sizeof(key), "center", i);  calibration[i].center = p.getInt(key, calibration[i].center);
    makeKey(key, sizeof(key), "max", i);     calibration[i].max    = p.getInt(key, calibration[i].max);
  }

  p.end();
  return true;
}

} // namespace detail

#endif

} // namespace joystickutilities
