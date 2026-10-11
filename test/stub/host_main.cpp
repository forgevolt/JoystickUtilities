// Runs an example sketch on a PC: setup() once, then loop() a few times.
#include "Arduino.h"

void setup();
void loop();

int main()
{
  setup();
  for (int i = 0; i < 20; ++i)
    loop();
  return 0;
}
