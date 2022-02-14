#ifndef WIFIHANDLER_H
#define WIFIHANDLER_H

#include "esp_event.h"
#include "esp_wifi.h"

#include "PowerManager.h"

/**
 * This class can create a WiFi AP with the given parameters
 */
class WiFiHandler {
   private:
    PowerManager* powerManager;
    wifi_config_t wifi_config;

   public:
    WiFiHandler(PowerManager* powerManager);

    static void wifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    void startAccessPoint();
    void stopAccessPoint();
};

#endif