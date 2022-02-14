#ifndef POWERMANAGER_H
#define POWERMANAGER_H

#include "LED.h"

/**
 * This class is used to manage the CPU powerdown
 */
class PowerManager {
   private:
    LED* redLED;

   public:
    PowerManager(LED* errorLED);
    void haltCPU();
};

#endif
