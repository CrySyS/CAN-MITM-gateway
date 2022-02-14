#include "WiFiHandler.h"

#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "defines.h"

WiFiHandler::WiFiHandler(PowerManager* powerManager) {
    this->powerManager = powerManager;

    /* Because of C++ / c99 difference, the following ap configuration has to be entered the way you see bellow. For more, please refer to the
    following searches: "error: C99 designator 'ssid' outside aggregate initializer"

    Original:
    wifi_ap_config_t ap = {.ssid = WIFI_SSID,
                           .password = WIFI_PASSWORD,
                           .ssid_len = strlen(WIFI_SSID),
                           .channel = 0,
                           .authmode = WIFI_AUTH_WPA_WPA2_PSK,
                           .ssid_hidden = 0,
                           .max_connection = WIFI_MAX_STA_CONNECTIONS,
                           .beacon_interval = 100};
    */
    WiFiHandler::wifi_config = wifi_config_t{
        .ap = {WIFI_SSID, WIFI_PASSWORD, strlen(WIFI_SSID), 0, WIFI_AUTH_WPA_WPA2_PSK, 0, WIFI_MAX_STA_CONNECTIONS, 100},
    };

    // Initialize NVS (Non-volatile storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        if (nvs_flash_erase() == ESP_OK) {
            ret = nvs_flash_init();
        } else {
            ESP_LOGE(WIFI_HANDLER_LOG_TAG, "Error during erasing the NVS, TERMINATING!");
            this->powerManager->haltCPU();
        }
    }
    if (ret != ESP_OK) {
        ESP_LOGE(WIFI_HANDLER_LOG_TAG, "Error during the NVS initialization, TERMINATING!");
        this->powerManager->haltCPU();
    }

    ESP_LOGD(WIFI_HANDLER_LOG_TAG, "ESP_WIFI_MODE_AP");
}

void WiFiHandler::wifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
        ESP_LOGI(WIFI_HANDLER_LOG_TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
        ESP_LOGI(WIFI_HANDLER_LOG_TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

void WiFiHandler::startAccessPoint() {
    tcpip_adapter_init();
    if (esp_event_loop_create_default() != ESP_OK) {
        ESP_LOGE(WIFI_HANDLER_LOG_TAG, "Error during creating the event loop, TERMINATING!");
        this->powerManager->haltCPU();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) {
        ESP_LOGE(WIFI_HANDLER_LOG_TAG, "Error during the wifi initialization, TERMINATING!");
        this->powerManager->haltCPU();
    }

    if (esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiHandler::wifiEventHandler, NULL) != ESP_OK) {
        ESP_LOGE(WIFI_HANDLER_LOG_TAG, "Error during the event handler registration, TERMINATING!");
        this->powerManager->haltCPU();
    }

    if (strlen(WIFI_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    if (esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK) {
        ESP_LOGE(WIFI_HANDLER_LOG_TAG, "Error during the WiFi mode selction, TERMINATING!");
        this->powerManager->haltCPU();
    }

    if (esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config) != ESP_OK) {
        ESP_LOGE(WIFI_HANDLER_LOG_TAG, "Error during the WiFi AP configuration, TERMINATING!");
        this->powerManager->haltCPU();
    }

    if (esp_wifi_start() != ESP_OK) {
        ESP_LOGE(WIFI_HANDLER_LOG_TAG, "Error during starting the WiFi AP, TERMINATING!");
        this->powerManager->haltCPU();
    }

    ESP_LOGI(WIFI_HANDLER_LOG_TAG, "Access Point startup finished. SSID:%s, password:%s", WIFI_SSID, WIFI_PASSWORD);
}

void WiFiHandler::stopAccessPoint() {
    if (esp_wifi_stop() != ESP_OK) {
        ESP_LOGE(WIFI_HANDLER_LOG_TAG, "Error during stopping the WiFi AP, TERMINATING!");
        this->powerManager->haltCPU();
    }

    if (esp_wifi_deinit() != ESP_OK) {
        ESP_LOGE(WIFI_HANDLER_LOG_TAG, "Error during the WiFi deinit, TERMINATING!");
        this->powerManager->haltCPU();
    }
}
