#include "ch.h"
#include "hal.h"
#include "device.h"

/*
 * Application entry point.
 */
int main(void) {
  
  halInit();
  chSysInit();

  // A wake from sleep is a software reset, and this pause made every wake 500 ms slower. Power-on keeps it so the
  // supply can settle (a power-on reset clears the software reset flag)
  if (!(RCC->CSR & RCC_CSR_SFTRSTF))
    chThdSleepMilliseconds(500);
  
  InitDevice();
  
  while (true) {
    chThdSleepMilliseconds(500);
  }
}
