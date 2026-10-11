// Host-side tests for JoystickUtilities. Build and run with:  make -C test
#include "JoystickUtilities.h"
#include "Preferences.h"

#include <cstdio>

using namespace joystickutilities;

namespace
{
int failures = 0;
int checks   = 0;

void check(bool ok, const char* what, int line)
{
  ++checks;
  if (!ok)
  {
    ++failures;
    printf("FAIL line %d: %s\n", line, what);
  }
}

#define CHECK(expr) check((expr), #expr, __LINE__)

// A read function that returns a fixed sequence, to see the averaging
int theSequence[4] = { 100, 200, 300, 400 };
int theSequenceIndex = 0;
int readSequence(uint8_t) { return theSequence[theSequenceIndex++ % 4]; }

// ----------------------------------------------------------------------------------------
void testRawReading()
{
  theAnalogValues[1] = 1000;
  AnalogAxis a(1);
  CHECK(a.readRaw() == 1000);

  AnalogAxis inv(1, true);
  CHECK(inv.readRaw() == 4095 - 1000);

  // Out-of-range readings are clamped to [0, adcMax]
  theAnalogValues[1] = 5000;
  CHECK(a.readRaw() == 4095);
  theAnalogValues[1] = -5;
  CHECK(a.readRaw() == 0);

  // Averaging: four samples of 100, 200, 300, 400
  theSequenceIndex = 0;
  AnalogAxis avg(2, false, AnalogConfig(), readSequence);
  CHECK(avg.readRaw() == 250);

  // samples < 1 is corrected to 1
  AnalogConfig one;
  one.samples = 0;
  theSequenceIndex = 0;
  AnalogAxis single(2, false, one, readSequence);
  CHECK(single.config().samples == 1);
  CHECK(single.readRaw() == 100);

  // A 10-bit ADC
  AnalogConfig tenBit;
  tenBit.adcMax = 1023;
  theAnalogValues[3] = 2000;
  AnalogAxis ten(3, false, tenBit);
  CHECK(ten.readRaw() == 1023);
  CHECK(ten.calibration().center == 511);
}

// ----------------------------------------------------------------------------------------
void testCalibrationCheck()
{
  AnalogAxis a(1);   // dead zone 50
  CHECK(a.config().deadzone == 50);
  CHECK(a.isUsable({ 0, 2048, 4095 }));
  CHECK(a.isUsable({ 1000, 1101, 1202 }));          // 1000 + 100 < 1101, 1101 + 100 < 1202
  CHECK(a.isUsable({ 1000, 1100, 1300 }) == false); // lower half too short
  CHECK(a.isUsable({ 1000, 1200, 1300 }) == false); // upper half too short
  CHECK(a.isUsable({ 2000, 2000, 2000 }) == false); // the stick was not moved
  CHECK(a.isUsable({ 3000, 2000, 1000 }) == false); // reversed

  // A rejected calibration falls back to the full range
  CHECK(a.setCalibration({ 500, 2000, 3500 }));
  CHECK(a.calibration().min == 500);
  CHECK(a.setCalibration({ 2000, 2000, 2000 }) == false);
  CHECK(a.calibration().min == 0);
  CHECK(a.calibration().center == 2047);
  CHECK(a.calibration().max == 4095);
}

// ----------------------------------------------------------------------------------------
// Every raw value against many calibrations: the result stays in [cMIN, cMAX], never
// decreases as the raw value rises, is 0 inside the dead zone and has the right sign outside
// it, and reaches full deflection at the calibrated ends. (Just outside the dead zone the
// result can still be 0: the first count past the edge maps to less than 1.)
void testMappingSweep()
{
  const int mins[]      = { 0, 100, 500, 1000 };
  const int centers[]   = { 1500, 2048, 2600 };
  const int maxs[]      = { 3000, 3600, 4095 };
  const int deadzones[] = { 0, 50, 80, 200 };

  long combinations = 0;
  int calibrations = 0;
  const uint8_t pin = 5;

  for (int dz : deadzones)
    for (int lo : mins)
      for (int c : centers)
        for (int hi : maxs)
          for (int inverted = 0; inverted < 2; inverted++)
          {
            AnalogConfig cfg;
            cfg.deadzone = dz;
            AnalogAxis a(pin, inverted == 1, cfg);
            if (a.isUsable({ lo, c, hi }) == false)
              continue;   // the check itself is tested above
            CHECK(a.setCalibration({ lo, c, hi }));
            calibrations++;

            bool inRange = true, monotonic = true, zeroInside = true, fullAtEnds = true;
            int previous = cMIN;
            for (int raw = 0; raw <= 4095; raw++)
            {
              // The axis mirrors the raw value when inverted, so feed the mirror image
              theAnalogValues[pin] = inverted ? 4095 - raw : raw;
              const int v = a.read();
              combinations++;

              inRange    &= (v >= cMIN && v <= cMAX);
              monotonic  &= (v >= previous);
              if (raw >= c - dz && raw <= c + dz) zeroInside &= (v == 0);
              else if (raw > c)                  zeroInside &= (v >= 0);
              else                               zeroInside &= (v <= 0);
              if (raw <= lo) fullAtEnds &= (v == cMIN);
              if (raw >= hi) fullAtEnds &= (v == cMAX);
              previous = v;
            }
            CHECK(inRange);
            CHECK(monotonic);
            CHECK(zeroInside);
            CHECK(fullAtEnds);
          }

  printf("mapping sweep: %d calibrations, %ld calibration/input combinations\n", calibrations, combinations);
}

// ----------------------------------------------------------------------------------------
void testDirectionKeys()
{
  DirectionKeys k;   // on 500, off 300, repeat after 1500 ms, then every 300 ms

  k.update(0, 0, 0);
  CHECK(!k.isUp() && !k.isDown() && !k.isLeft() && !k.isRight());

  // One press on the way in, nothing while held below the repeat delay
  k.update(0, 600, 10);
  CHECK(k.isUp());
  k.update(0, 600, 20);
  CHECK(k.isUp() == false);

  // Hysteresis: 400 is below "on" but above "off", so it stays held without a new press
  k.update(0, 400, 30);
  CHECK(k.isUp() == false);

  // First repeat after 1500 ms, then every 300 ms
  k.update(0, 600, 1500);
  CHECK(k.isUp() == false);
  k.update(0, 600, 1511);
  CHECK(k.isUp());
  k.update(0, 600, 1700);
  CHECK(k.isUp() == false);
  k.update(0, 600, 1812);
  CHECK(k.isUp());

  // Release and press again: an immediate new press
  k.update(0, 200, 1900);
  CHECK(k.isUp() == false);
  k.update(0, 600, 1910);
  CHECK(k.isUp());

  // The other three directions
  k.update(0, -600, 2000);  CHECK(k.isDown());
  k.update(600, 0, 2010);   CHECK(k.isRight());
  k.update(-600, 0, 2020);  CHECK(k.isLeft());

  // Wrapping millis() does not break the timing
  DirectionKeys w;
  const unsigned long nearWrap = 0xFFFFFFFFUL - 100;
  w.update(0, 600, nearWrap);
  CHECK(w.isUp());
  w.update(0, 600, nearWrap + 1600);   // wrapped past 0
  CHECK(w.isUp());
}

// ----------------------------------------------------------------------------------------
void testJoystick()
{
  theAnalogValues[10] = 4095;
  theAnalogValues[11] = 0;
  theAnalogValues[12] = 2047;
  Joystick<3> s({ AnalogAxis(10), AnalogAxis(11), AnalogAxis(12) });
  s.begin();
  s.update(0);
  CHECK(s.getX() == cMAX);
  CHECK(s.getY() == cMIN);
  CHECK(s.getZ() == 0);
  CHECK(s.get(1) == cMIN);
  CHECK(s.get(7) == 0);          // out of range
  CHECK(s.isRight());
  CHECK(s.isDown());

  // Calibration result per axis
  auto r = s.setCalibration({ AxisCalibration{ 100, 2000, 3900 },
                              AxisCalibration{ 2000, 2000, 2000 },
                              AxisCalibration{ 100, 2000, 3900 } });
  CHECK(r.accepted[0] && !r.accepted[1] && r.accepted[2]);
  CHECK(r.allAccepted() == false);
  CHECK(s.calibration()[0].min == 100);
  CHECK(s.calibration()[1].min == 0);

  // Inactive: zeros, no raw reads, calibrations ignored and reported as accepted
  s.setActive(false);
  CHECK(s.getX() == 0);
  s.update(10);
  CHECK(s.getX() == 0 && s.getY() == 0);
  CHECK(s.readRaw(0) == 0);
  auto r2 = s.setCalibration({ AxisCalibration{ 2000, 2000, 2000 },
                               AxisCalibration{ 2000, 2000, 2000 },
                               AxisCalibration{ 2000, 2000, 2000 } });
  CHECK(r2.allAccepted());
  CHECK(s.calibration()[0].min == 100);   // unchanged

  s.setActive(true);
  s.update(20);
  CHECK(s.getX() == cMAX);

  // One axis: no direction keys, nothing else breaks
  Joystick<1> one({ AnalogAxis(10) });
  one.update(0);
  CHECK(one.getX() == cMAX);
}

// ----------------------------------------------------------------------------------------
void testCalibrationBuildingBlocks()
{
  Joystick<2> s({ AnalogAxis(20), AnalogAxis(21) });

  CenterSampler<2> center;
  CHECK(center.count() == 0);
  CHECK(center.center()[0] == 0);
  theAnalogValues[20] = 2000; theAnalogValues[21] = 2100; center.add(s);
  theAnalogValues[20] = 2001; theAnalogValues[21] = 2100; center.add(s);
  theAnalogValues[20] = 2003; theAnalogValues[21] = 2101; center.add(s);
  CHECK(center.count() == 3);
  CHECK(center.center()[0] == 2001);   // 6004 / 3 = 2001.3
  CHECK(center.center()[1] == 2100);   // 6301 / 3 = 2100.3

  RangeTracker<2> range(center.center());
  CHECK(range.min(0) == 2001 && range.max(0) == 2001 && range.center(0) == 2001);
  theAnalogValues[20] = 150;  theAnalogValues[21] = 2100; range.add(s);
  theAnalogValues[20] = 3950; theAnalogValues[21] = 2110; range.add(s);
  CHECK(range.min(0) == 150 && range.max(0) == 3950);
  CHECK(range.min(1) == 2100 && range.max(1) == 2110);

  // x was swept, y was not
  auto r = s.setCalibration(range.calibration());
  CHECK(r.accepted[0] && !r.accepted[1]);

  // An inactive stick adds nothing
  s.setActive(false);
  CenterSampler<2> none;
  none.add(s);
  CHECK(none.count() == 0);
  RangeTracker<2> rangeNone({ 2000, 2000 });
  theAnalogValues[20] = 0;
  rangeNone.add(s);
  CHECK(rangeNone.min(0) == 2000);

  center.reset();
  CHECK(center.count() == 0);
}

// ----------------------------------------------------------------------------------------
void testStorage()
{
  Preferences::storage().clear();

  Joystick<3> s({ AnalogAxis(30), AnalogAxis(31), AnalogAxis(32) });

  // Nothing stored: false, defaults kept, and the namespace is not created
  CHECK(loadCalibration(s, "Joystick36") == false);
  CHECK(Preferences::storage().count("Joystick36") == 0);
  CHECK(s.calibration()[0].max == 4095);

  // Save and load again
  s.setCalibration({ AxisCalibration{ 100, 2000, 3900 },
                     AxisCalibration{ 200, 2100, 3800 },
                     AxisCalibration{ 300, 2200, 3700 } });
  CHECK(saveCalibration(s, "Joystick36"));

  // The keys the existing sketches use
  Preferences::Namespace& ns = Preferences::storage()["Joystick36"];
  CHECK(ns["minX"] == 100 && ns["centerY"] == 2100 && ns["maxZ"] == 3700);

  Joystick<3> t({ AnalogAxis(30), AnalogAxis(31), AnalogAxis(32) });
  CalibrationResult<3> r;
  CHECK(loadCalibration(t, "Joystick36", &r));
  CHECK(r.allAccepted());
  CHECK(t.calibration()[1].center == 2100);

  // Applied to an inactive stick too
  Joystick<3> off({ AnalogAxis(30), AnalogAxis(31), AnalogAxis(32) });
  off.setActive(false);
  CHECK(loadCalibration(off, "Joystick36"));
  CHECK(off.calibration()[2].min == 300);

  // A stored calibration that fails the check (e.g. a smaller dead zone was in use when it
  // was saved) is reported and replaced by the full range
  ns["minY"] = 2050;
  CHECK(loadCalibration(t, "Joystick36", &r));
  CHECK(r.accepted[0] && !r.accepted[1] && r.accepted[2]);
  CHECK(t.calibration()[1].min == 0);

  // Missing keys keep the current values
  Preferences::storage()["Joystick2"]["minX"] = 150;
  Joystick<2> u({ AnalogAxis(30), AnalogAxis(31) });
  CHECK(loadCalibration(u, "Joystick2"));
  CHECK(u.calibration()[0].min == 150 && u.calibration()[0].max == 4095);
}

} // namespace

// ----------------------------------------------------------------------------------------
int main()
{
  testRawReading();
  testCalibrationCheck();
  testMappingSweep();
  testDirectionKeys();
  testJoystick();
  testCalibrationBuildingBlocks();
  testStorage();

  printf("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
