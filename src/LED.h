#ifndef LED_H
#define LED_H

#include "driver/gpio.h"

/**
 * This class can drive an LED
 */
class LED {
   private:
    gpio_num_t pinNumber;
    bool currentState;

   public:
    LED();
    LED(gpio_num_t pinNumber);
    void setState(bool state);
    bool getState();
};

#endif