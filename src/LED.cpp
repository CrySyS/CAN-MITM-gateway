#include "LED.h"

#include "driver/gpio.h"

LED::LED() {}

LED::LED(gpio_num_t pinNumber) {
    this->pinNumber = pinNumber;  // store the number

    gpio_pad_select_gpio(pinNumber);                  // select the pin as GPIO pin
    gpio_set_direction(pinNumber, GPIO_MODE_OUTPUT);  // set the direction of the pin
    this->setState(false);                            // turn off the LED
}

void LED::setState(bool state) {
    this->currentState = state;
    gpio_set_level(this->pinNumber, state);  // set the new state
}

bool LED::getState(){
    return this->currentState;
}