#include "ch.h"
#include "hal.h"
#include "device.h"

/*
 * Application entry point.
 */
int main(void) {
  
  halInit();
  chSysInit();

  // No start-up pause: a wake from sleep is a reset, and 500 ms here made every wake that much slower. At power-on
  // the bootloader's own update window already lets the supply settle.
  InitDevice();
  
  while (true) {
    chThdSleepMilliseconds(500);
  }
}
