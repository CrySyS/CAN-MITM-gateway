#include "RoleSelector.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "LED.h"
#include "PowerState.h"
#include "defines.h"

RoleSelector::RoleSelector(LED* masterLED, LED* slaveLED) {
    gpio_pad_select_gpio(ROLE_PIN);                  // select the pin as GPIO pin
    gpio_pad_select_gpio(STARTUP_FINISHED_ACK_PIN);  // select the pin as GPIO pin
    gpio_set_direction(ROLE_PIN, GPIO_MODE_INPUT);   // set the direction of the pin
    int roleValue = gpio_get_level(ROLE_PIN);

    if (roleValue == 0) {  // If the ROLE_PIN is low, we're the master
        this->selectedRole = Role::MASTER;
        masterLED->setState(true);
        ESP_LOGI(ROLE_SELECTOR_LOG_TAG, "CONSTRUCTOR - role is set to MASTER");
        
        gpio_set_direction(STARTUP_FINISHED_ACK_PIN, GPIO_MODE_OUTPUT);  // set the direction of the pin
        gpio_set_level(STARTUP_FINISHED_ACK_PIN, 0); //we set the signal that the setup is still in progress

        gpio_set_level(RESET_SLAVE_PIN, (uint32_t)PowerState::IN_WORKING_ORDER);  // release the slave from the reset
        ESP_LOGD(ROLE_SELECTOR_LOG_TAG, "CONSTRUCTOR - SLAVE released from reset");
    } else {
        this->selectedRole = Role::SLAVE;  // The master already set the ROLE_PIN to high, we're the slave
        slaveLED->setState(true);

        gpio_set_direction(STARTUP_FINISHED_ACK_PIN, GPIO_MODE_INPUT);  // set the direction of the pin
        ESP_LOGI(ROLE_SELECTOR_LOG_TAG, "CONSTRUCTOR - role is set to SLAVE");
    }
}

Role& RoleSelector::getRole() {
    return this->selectedRole;
}

void RoleSelector::holdSlaveInReset(){
    gpio_pad_select_gpio(RESET_SLAVE_PIN);                                 // select the pin as GPIO pin
    gpio_set_direction(RESET_SLAVE_PIN, GPIO_MODE_OUTPUT);                 // set the direction of the pin
    gpio_set_level(RESET_SLAVE_PIN, (uint32_t)PowerState::HELD_IN_RESET);  // pull the reset pin to high
}