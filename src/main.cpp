#include <stdint.h>
#include <stdio.h>

#include "bootloader_random.h"
#include "esp_log.h"  //This has to be included before defines.h in order to make the LOG_LEVEL selection take effect
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "AttackConfig.h"
#include "AttackType.h"
#include "CANHandler.h"
#include "LED.h"
#include "MessageAttacker.h"
#include "PowerManager.h"
#include "Role.h"
#include "RoleSelector.h"
#include "UARTHandler.h"
#include "WebServerHandler.h"
#include "WiFiHandler.h"
#include "defines.h"

extern "C" void app_main(void) {
    ESP_LOGI(MAIN_LOG_TAG, "Trying to reset slave");
    RoleSelector::holdSlaveInReset();

    ESP_LOGI(MAIN_LOG_TAG, "Initializing components");

    ESP_LOGD(MAIN_LOG_TAG, "Setting up the LEDs");
    LED masterLED(BLUE_LED_PIN);
    LED slaveLED(YELLOW_LED_PIN);
    LED errorLED(RED_LED_PIN);
    errorLED.setState(true);  // Signal that we're not okay yet

    ESP_LOGD(MAIN_LOG_TAG, "Enabling the hardware entropy source for the RNG");  // required for the REPLACE_DATA_WITH_RANDOM_VALUES attack
    bootloader_random_enable();

    ESP_LOGD(MAIN_LOG_TAG, "Setting up the RoleSelector");
    RoleSelector roleSelector(&masterLED, &slaveLED);

    ESP_LOGD(MAIN_LOG_TAG, "Setting up the PowerManager");
    PowerManager powerManager(&errorLED);

    ESP_LOGD(MAIN_LOG_TAG, "Setting up the UARTHandler");
    UARTHandler uartHandler(&powerManager, roleSelector.getRole());

    ESP_LOGD(MAIN_LOG_TAG, "Creating a dummy attack config");
    AttackConfig attackConfig;

    ESP_LOGD(MAIN_LOG_TAG, "Setting up the MessageAttacker with dummy attackConfig");
    MessageAttacker messageAttacker(&attackConfig);

    ESP_LOGD(MAIN_LOG_TAG, "Setting up the CANHandler, with a placeholder speed");
    CANHandler canHandler(&uartHandler, &powerManager, &messageAttacker, roleSelector.getRole());

    ESP_LOGI(MAIN_LOG_TAG, "Components initialized");

    if(roleSelector.getRole() == Role::MASTER){
        ESP_LOGD(MAIN_LOG_TAG, "Setting up the WiFi Access Point");
        WiFiHandler wifiHandler(&powerManager);

        ESP_LOGI(MAIN_LOG_TAG, "Starting the WiFi Access Point");
        wifiHandler.startAccessPoint();

        ESP_LOGD(MAIN_LOG_TAG, "Setting up the Webserver");
        WebServerHandler webServerHandler(&powerManager, &attackConfig);

        ESP_LOGD(MAIN_LOG_TAG, "Starting the Webserver");
        webServerHandler.startWebServer();

        ESP_LOGI(MAIN_LOG_TAG, "Webserver started, waiting for configuration on 192.168.4.1");
        bool configReceived = false;
        do {
            configReceived = webServerHandler.checkIfConfigReceived();
            if(configReceived){
                ESP_LOGI(MAIN_LOG_TAG, "New config received");
                masterLED.setState(true);

                ESP_LOGD(MAIN_LOG_TAG, "Setting the new attack config");
                messageAttacker.updateAttackConfig();

                ESP_LOGD(MAIN_LOG_TAG, "Reconfiguring the CAN driver with the new bitrate");
                canHandler.setNewBitrate(attackConfig.getBitrate());

                ESP_LOGI(MAIN_LOG_TAG, "Sending new CAN bitrate to slave");
                uartHandler.sendNewCANBitrateToSlave(attackConfig.getBitrate());

                ESP_LOGD(MAIN_LOG_TAG, "Waiting for the slave to set the new CAN bitrate");
                vTaskDelay(NEW_CAN_BITRATE_SENT_DELAY_IN_MILLIS / portTICK_PERIOD_MS);
            } else {
                ESP_LOGV(MAIN_LOG_TAG, "Waiting for new attack config to be set");
                masterLED.setState(!masterLED.getState());
                vTaskDelay(LED_BLINK_INTERVAL_DURING_WIFI_CONFIG_IN_MILLIS / portTICK_PERIOD_MS);
            }
        } while (!configReceived);

        ESP_LOGI(MAIN_LOG_TAG, "Stopping the Webserver");
        webServerHandler.stopWebServer();

        ESP_LOGI(MAIN_LOG_TAG, "Shutting down the WiFi Access Point");
        wifiHandler.stopAccessPoint();

        ESP_LOGD(MAIN_LOG_TAG, "Clearing UART buffer");
        uartHandler.emptyBuffer();

        ESP_LOGD(MAIN_LOG_TAG, "Signalling ready state to slave");
        gpio_set_level(STARTUP_FINISHED_ACK_PIN, 1);  // signal that we are ready
    } else {
        vTaskDelay(LAST_UART_TEST_MESSAGE_ARRIVE_DELAY_IN_MILLIS /
                   portTICK_PERIOD_MS);  // Since the MASTER still has to be configured through WiFi, we can delay a little bit of time before
                                         // empyting the buffer in order to leave a time frame in case a "residue" message would still arrive after
                                         // the UART test due to timing and concurrency.
        ESP_LOGD(MAIN_LOG_TAG, "Clearing UART buffer");
        uartHandler.emptyBuffer();

        ESP_LOGI(MAIN_LOG_TAG, "Waiting for the master to be ready for start");
        bool newCANBitrateReceived = false;
        while (gpio_get_level(STARTUP_FINISHED_ACK_PIN) != 1) {  // While we don't get the signal from the MASTER, that the configuration has been finished
            int newBitrate = 0;

            if (uartHandler.receiveNewCANBitrateSettings(newBitrate)) {
                ESP_LOGI(MAIN_LOG_TAG, "New CAN bitrate setting received");
                canHandler.setNewBitrate(newBitrate);
                slaveLED.setState(true);
                newCANBitrateReceived = true;

                vTaskDelay(NEW_CAN_BITRATE_RECEIVED_UART_BUFFER_EMPTYING_DELAY_IN_MILLIS / portTICK_PERIOD_MS);
                ESP_LOGD(MAIN_LOG_TAG, "Clearing UART buffer");
                uartHandler.emptyBuffer();
            }

            slaveLED.setState(!slaveLED.getState() || newCANBitrateReceived); //if we've received the new bitrate, we stay permanently on
            vTaskDelay(WAIT_FOR_MASTER_TO_BE_CONFIGURED_DELAY_IN_MILLIS / portTICK_PERIOD_MS);  // delay the task
        }
        ESP_LOGD(MAIN_LOG_TAG, "Master is ready");
    }

    ESP_LOGD(MAIN_LOG_TAG, "Starting CAN Handler");
    canHandler.startHandlingCANMessages();

    ESP_LOGI(MAIN_LOG_TAG, "Setup finished!");
    errorLED.setState(false);  // setup finished, time to turn of the error LED
}  //At this point, the main task terminates, and only the receive and send tasks run paralel on the two cores.