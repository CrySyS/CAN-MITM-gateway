#include "PowerManager.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "LED.h"
#include "defines.h"

PowerManager::PowerManager(LED* errorLED) {
    this->redLED = errorLED;
}

void PowerManager::haltCPU() {
    ESP_LOGE(POWER_MANAGER_LOG_TAG, "Halting CPU");
    this->redLED->setState(true);

    ESP_LOGD(POWER_MANAGER_LOG_TAG, "Entering deep sleep");
    esp_deep_sleep_start();
}