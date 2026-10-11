#pragma once

// JoystickUtilities - reading and calibrating physical joysticks on Arduino
// https://github.com/forgevolt/JoystickUtilities
//
// Copyright (c) 2026 Christoph Streit. MIT license, see LICENSE.
//
// Reads analog thumb sticks (one potentiometer per axis) and maps each axis to [-1000, 1000]
// with a dead zone around the center. Provides the building blocks of a calibration - averaging
// the resting position, recording the travel while the stick is swept, checking the result -
// but not the procedure: when each step starts and ends, and what the user sees, is up to the
// sketch. Optional direction "keys" with auto-repeat turn a stick into up/down/left/right
// events, e.g. for menus.
//
// This library reads joysticks. It does not make a board appear as a USB game controller.
//
// 32-bit boards only (uses <array>), C++11 or later. Storing calibrations uses Preferences and
// is ESP32-only; everything else is board-independent.

#include <Arduino.h>
#include <array>
#include <type_traits>
#include <stddef.h>
#include <stdint.h>

namespace joystickutilities
{

// ---- Values -----------------------------------------------------------------------------

constexpr int cMIN = -1000;   // full deflection one way
constexpr int cMAX =  1000;   // full deflection the other way; 0 is the center

// Reads one raw ADC value from a pin. Defaults to analogRead(); pass your own for an external
// ADC, a multiplexer, or tests.
using ReadFunction = int (*)(uint8_t pin);

// ---- AnalogAxis -------------------------------------------------------------------------

// Settings of one analog axis. The defaults suit an ESP32 (12-bit ADC) with a typical
// thumb stick.
struct AnalogConfig
{
  int adcMax   = 4095;  // highest raw value the read function returns (4095 = 12 bit)
  int deadzone = 50;    // raw counts around the center that read as 0 (about 1.2 % of the range)
  int samples  = 4;     // readings averaged per read, at least 1
};

// The calibration of one axis, in raw ADC counts (after inversion).
struct AxisCalibration
{
  int min;
  int center;
  int max;
};

// One potentiometer axis: reading, averaging, inversion, calibration and mapping.
class AnalogAxis
{
  public:
    explicit AnalogAxis(uint8_t pin, bool inverted = false,
                        const AnalogConfig& config = AnalogConfig(), ReadFunction readFunction = nullptr);

    // Nothing to set up for an analog pin today; kept so that every axis type has it.
    void begin();

    // Averaged raw value in [0, adcMax]; mirrored (adcMax - value) when inverted.
    uint16_t readRaw() const;

    // Raw value mapped to [cMIN, cMAX]: 0 inside the dead zone, each half mapped separately
    // from the edge of the dead zone to the calibrated end, clamped beyond it.
    int read() const;

    // A calibration is usable when both halves of the travel clear the dead zone with room to
    // spare (min + 2*deadzone < center and center + 2*deadzone < max). Anything less would map
    // to a stuck full deflection - an axis left untouched during the sweep is the typical case.
    bool isUsable(const AxisCalibration& calibration) const;

    // Applies a usable calibration and returns true. An unusable one is not applied: the axis
    // falls back to the full ADC range ({0, adcMax/2, adcMax}) and the call returns false.
    bool setCalibration(const AxisCalibration& calibration);

    // The full-range default, which is also the state before any calibration.
    void resetCalibration();

    const AxisCalibration& calibration() const { return myCalibration; }
    const AnalogConfig&    config()      const { return myConfig; }
    uint8_t                pin()         const { return myPin; }

  private:
    uint8_t         myPin;
    bool            myInverted;
    AnalogConfig    myConfig;
    ReadFunction    myRead;
    AxisCalibration myCalibration;
};

// ---- DirectionKeys ----------------------------------------------------------------------

// Thresholds are deflections on the axis scale of Joystick::get(), measured from the center
// towards each direction: 500 means half-way to the end stop, also for left and down.
struct DirectionConfig
{
  int onThreshold  = 500;   // deflection at which a direction fires (500 = half-way)
  int offThreshold = 300;   // deflection below which it is released again; the gap to
                            // onThreshold keeps a stick held near the threshold from chattering
  unsigned long repeatDelayMs    = 1500; // held this long (ms), the direction starts repeating ...
  unsigned long repeatIntervalMs = 300;  // ... and then fires again every this many ms
};

// Turns an x/y pair into up/down/left/right "key presses": true on the edge into a direction
// and again on each auto-repeat while it is held. Positive y is up, positive x is right.
class DirectionKeys
{
  public:
    explicit DirectionKeys(const DirectionConfig& config = DirectionConfig());

    // Advances all four directions from one sample. All four advance on every call, so holding
    // one does not leave the others' timers stale.
    void update(int x, int y, unsigned long nowMs);

    bool isUp()    const { return myUp;    }
    bool isDown()  const { return myDown;  }
    bool isLeft()  const { return myLeft;  }
    bool isRight() const { return myRight; }

    void setConfig(const DirectionConfig& config) { myConfig = config; }

  private:
    struct State
    {
      bool active = false;
      bool repeating = false;
      unsigned long since = 0;
      unsigned long lastRepeat = 0;
    };

    bool step(int value, State& state, unsigned long nowMs);

    DirectionConfig myConfig;
    State myUpState, myDownState, myLeftState, myRightState;
    bool myUp = false, myDown = false, myLeft = false, myRight = false;
};

// ---- Joystick ---------------------------------------------------------------------------

// Per axis: true if the calibration was applied, false if the axis fell back to the full range.
template <size_t N>
struct CalibrationResult
{
  std::array<bool, N> accepted;

  bool allAccepted() const;
};

// A stick with N axes of type Axis (AnalogAxis today; a switch-based axis can follow).
// Axis 0 is x, 1 is y, 2 is z. Values are sampled once per update() and reported until the
// next one, so everything that reads the stick in one pass sees the same numbers.
template <size_t N, typename Axis = AnalogAxis>
class Joystick
{
  static_assert(N >= 1 && N <= 6, "Joystick supports 1 to 6 axes");

  public:
    explicit Joystick(const std::array<Axis, N>& axes,
                      const DirectionConfig& directions = DirectionConfig());

    void begin();

    // Samples every axis and, with two or more axes, advances the direction keys from x and y.
    void update();
    void update(unsigned long nowMs);    // the same with an explicit time, e.g. for tests

    // Values from the last update(), in [cMIN, cMAX].
    int get(size_t axis) const;
    int getX() const;
    int getY() const;                    // N >= 2
    int getZ() const;                    // N >= 3

    // Direction keys from the last update() (N >= 2).
    bool isUp() const;
    bool isDown() const;
    bool isLeft() const;
    bool isRight() const;

    // A stick that is not fitted (e.g. the second stick of a one-stick build) can be switched
    // off: it then reports 0 everywhere, reads 0 raw without touching the ADC, and ignores
    // calibrations. On by default.
    void setActive(bool active);
    bool isActive() const;

    // Applies one calibration per axis; see AnalogAxis::setCalibration(). An inactive stick
    // leaves its calibration alone and reports every axis as accepted.
    CalibrationResult<N> setCalibration(const std::array<AxisCalibration, N>& calibration);
    std::array<AxisCalibration, N> calibration() const;

    // Raw value of one axis (0 when inactive), e.g. for showing the travel during calibration.
    uint16_t readRaw(size_t axis) const;

    Axis&       axis(size_t index);
    const Axis& axis(size_t index) const;

  private:
    // Direction keys need x and y, so a one-axis stick has none. Chosen at compile time; a C++11
    // stand-in for 'if constexpr'.
    template <size_t M = N>
    typename std::enable_if<(M >= 2)>::type updateDirections(unsigned long nowMs)
    {
      myDirections.update(myValues[0], myValues[1], nowMs);
    }

    template <size_t M = N>
    typename std::enable_if<(M < 2)>::type updateDirections(unsigned long) {}

    std::array<Axis, N> myAxes;
    std::array<int, N>  myValues {};
    DirectionKeys       myDirections;
    bool                myActive = true;
};

// ---- Calibration building blocks --------------------------------------------------------
//
// Neither knows about time, buttons or a display: the sketch decides when to add samples and
// when a step is over. A typical calibration:
//
//   CenterSampler<2> center;      // the stick is left alone
//   ... center.add(stick) for a moment ...
//   RangeTracker<2> range(center.center());
//   ... range.add(stick) while the user sweeps every axis to its ends ...
//   auto result = stick.setCalibration(range.calibration());

// Averages the resting position of each axis.
template <size_t N>
class CenterSampler
{
  public:
    void reset();

    // Adds one raw sample of every axis. Ignored for an inactive stick.
    template <typename Axis>
    void add(const Joystick<N, Axis>& stick);

    size_t count() const;

    // Rounded average per axis. Before the first sample: the middle of the ADC range is not
    // known here, so check count() first.
    std::array<int, N> center() const;

  private:
    std::array<long, N> mySums {};
    size_t myCount = 0;
};

// Records how far each axis travels while the stick is swept.
template <size_t N>
class RangeTracker
{
  public:
    // Starts with min = max = center for every axis.
    explicit RangeTracker(const std::array<int, N>& center);

    // Widens min/max of every axis by one raw sample. Ignored for an inactive stick.
    template <typename Axis>
    void add(const Joystick<N, Axis>& stick);

    int min(size_t axis) const;
    int max(size_t axis) const;
    int center(size_t axis) const;

    // The result as calibrations, ready for Joystick::setCalibration().
    std::array<AxisCalibration, N> calibration() const;

  private:
    std::array<AxisCalibration, N> myRange;
};

// ---- Storing calibrations (ESP32) -------------------------------------------------------
//
// Keys are min<A>, center<A>, max<A> per axis, with <A> = X, Y, Z, U, V, W for axes 0-5 - the
// keys the Transmitter and VectorArcade sketches already use, so their stored calibrations stay
// valid. The namespace is the caller's: one per stick (at most 15 characters).

#if defined(ESP32)

// Writes the stick's current calibration. Returns false if the namespace could not be opened.
template <size_t N, typename Axis>
bool saveCalibration(const Joystick<N, Axis>& stick, const char* nameSpace);

// Reads a stored calibration and applies it with the usual check. Opens the namespace
// read-only, so a stick that was never calibrated leaves NVS untouched. Returns false if
// nothing was stored (the stick then keeps its full-range defaults); otherwise the result
// says which axes were accepted. Applied even to an inactive stick, so switching it on later
// finds its calibration.
template <size_t N, typename Axis>
bool loadCalibration(Joystick<N, Axis>& stick, const char* nameSpace,
                     CalibrationResult<N>* result = nullptr);

namespace detail
{
// The non-template part of save/load, in JoystickUtilities.cpp. load() starts from the values
// in 'calibration' and overwrites only the keys that exist.
bool saveCalibrations(const char* nameSpace, const AxisCalibration* calibration, size_t count);
bool loadCalibrations(const char* nameSpace, AxisCalibration* calibration, size_t count);
}

#endif

// ---- Template definitions ---------------------------------------------------------------
// The non-template code is in JoystickUtilities.cpp.

template <size_t N>
bool CalibrationResult<N>::allAccepted() const
{
  for (bool a : accepted)
    if (a == false)
      return false;
  return true;
}

// ---- Joystick

template <size_t N, typename Axis>
Joystick<N, Axis>::Joystick(const std::array<Axis, N>& axes, const DirectionConfig& directions)
: myAxes(axes), myDirections(directions)
{}

template <size_t N, typename Axis>
void Joystick<N, Axis>::begin()
{
  for (Axis& a : myAxes)
    a.begin();
}

template <size_t N, typename Axis>
void Joystick<N, Axis>::update()
{
  update(millis());
}

template <size_t N, typename Axis>
void Joystick<N, Axis>::update(unsigned long nowMs)
{
  for (size_t i = 0; i < N; i++)
    myValues[i] = myActive ? myAxes[i].read() : 0;

  updateDirections(nowMs);
}

template <size_t N, typename Axis>
int Joystick<N, Axis>::get(size_t axis) const
{
  return (axis < N) ? myValues[axis] : 0;
}

template <size_t N, typename Axis>
int Joystick<N, Axis>::getX() const
{
  return myValues[0];
}

template <size_t N, typename Axis>
int Joystick<N, Axis>::getY() const
{
  static_assert(N >= 2, "getY() needs at least 2 axes");
  return myValues[1];
}

template <size_t N, typename Axis>
int Joystick<N, Axis>::getZ() const
{
  static_assert(N >= 3, "getZ() needs at least 3 axes");
  return myValues[2];
}

template <size_t N, typename Axis>
bool Joystick<N, Axis>::isUp() const
{
  static_assert(N >= 2, "direction keys need at least 2 axes");
  return myDirections.isUp();
}

template <size_t N, typename Axis>
bool Joystick<N, Axis>::isDown() const
{
  static_assert(N >= 2, "direction keys need at least 2 axes");
  return myDirections.isDown();
}

template <size_t N, typename Axis>
bool Joystick<N, Axis>::isLeft() const
{
  static_assert(N >= 2, "direction keys need at least 2 axes");
  return myDirections.isLeft();
}

template <size_t N, typename Axis>
bool Joystick<N, Axis>::isRight() const
{
  static_assert(N >= 2, "direction keys need at least 2 axes");
  return myDirections.isRight();
}

template <size_t N, typename Axis>
void Joystick<N, Axis>::setActive(bool active)
{
  myActive = active;
  if (active == false)
    myValues.fill(0);
}

template <size_t N, typename Axis>
bool Joystick<N, Axis>::isActive() const
{
  return myActive;
}

template <size_t N, typename Axis>
CalibrationResult<N> Joystick<N, Axis>::setCalibration(const std::array<AxisCalibration, N>& calibration)
{
  CalibrationResult<N> result;
  result.accepted.fill(true);

  if (myActive == false)
    return result;

  for (size_t i = 0; i < N; i++)
    result.accepted[i] = myAxes[i].setCalibration(calibration[i]);

  return result;
}

template <size_t N, typename Axis>
std::array<AxisCalibration, N> Joystick<N, Axis>::calibration() const
{
  std::array<AxisCalibration, N> c;
  for (size_t i = 0; i < N; i++)
    c[i] = myAxes[i].calibration();
  return c;
}

template <size_t N, typename Axis>
uint16_t Joystick<N, Axis>::readRaw(size_t axis) const
{
  return (myActive && axis < N) ? myAxes[axis].readRaw() : 0;
}

template <size_t N, typename Axis>
Axis& Joystick<N, Axis>::axis(size_t index)
{
  return myAxes[index];
}

template <size_t N, typename Axis>
const Axis& Joystick<N, Axis>::axis(size_t index) const
{
  return myAxes[index];
}

// ---- CenterSampler

template <size_t N>
void CenterSampler<N>::reset()
{
  mySums.fill(0);
  myCount = 0;
}

template <size_t N>
template <typename Axis>
void CenterSampler<N>::add(const Joystick<N, Axis>& stick)
{
  if (stick.isActive() == false)
    return;

  for (size_t i = 0; i < N; i++)
    mySums[i] += stick.readRaw(i);
  myCount++;
}

template <size_t N>
size_t CenterSampler<N>::count() const
{
  return myCount;
}

template <size_t N>
std::array<int, N> CenterSampler<N>::center() const
{
  std::array<int, N> c {};
  if (myCount == 0)
    return c;

  for (size_t i = 0; i < N; i++)
    c[i] = static_cast<int>((mySums[i] + static_cast<long>(myCount / 2)) / static_cast<long>(myCount));
  return c;
}

// ---- RangeTracker

template <size_t N>
RangeTracker<N>::RangeTracker(const std::array<int, N>& center)
{
  for (size_t i = 0; i < N; i++)
    myRange[i] = { center[i], center[i], center[i] };
}

template <size_t N>
template <typename Axis>
void RangeTracker<N>::add(const Joystick<N, Axis>& stick)
{
  if (stick.isActive() == false)
    return;

  for (size_t i = 0; i < N; i++)
  {
    const int v = stick.readRaw(i);
    if (v < myRange[i].min) myRange[i].min = v;
    if (v > myRange[i].max) myRange[i].max = v;
  }
}

template <size_t N>
int RangeTracker<N>::min(size_t axis) const
{
  return (axis < N) ? myRange[axis].min : 0;
}

template <size_t N>
int RangeTracker<N>::max(size_t axis) const
{
  return (axis < N) ? myRange[axis].max : 0;
}

template <size_t N>
int RangeTracker<N>::center(size_t axis) const
{
  return (axis < N) ? myRange[axis].center : 0;
}

template <size_t N>
std::array<AxisCalibration, N> RangeTracker<N>::calibration() const
{
  return myRange;
}

// ---- Storing calibrations

#if defined(ESP32)

template <size_t N, typename Axis>
bool saveCalibration(const Joystick<N, Axis>& stick, const char* nameSpace)
{
  const std::array<AxisCalibration, N> c = stick.calibration();
  return detail::saveCalibrations(nameSpace, c.data(), N);
}

template <size_t N, typename Axis>
bool loadCalibration(Joystick<N, Axis>& stick, const char* nameSpace, CalibrationResult<N>* result)
{
  std::array<AxisCalibration, N> c = stick.calibration();
  if (detail::loadCalibrations(nameSpace, c.data(), N) == false)
    return false;

  // Straight to the axes rather than through Joystick::setCalibration(), which an inactive
  // stick ignores.
  CalibrationResult<N> r;
  for (size_t i = 0; i < N; i++)
    r.accepted[i] = stick.axis(i).setCalibration(c[i]);

  if (result != nullptr)
    *result = r;
  return true;
}

#endif

} // namespace joystickutilities
