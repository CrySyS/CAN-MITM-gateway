#include "CANHandler.h"

#include <string.h>

#include "driver/can.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "MessageAttacker.h"
#include "PowerManager.h"
#include "Role.h"
#include "UARTHandler.h"
#include "defines.h"

//The handler tasks run separated from the object, thus every related variable has to be static.
int CANHandler::usefulSizeOfACANMessageTypeVariable;
can_timing_config_t CANHandler::timingConfig;
can_filter_config_t CANHandler::filterConfig;
can_general_config_t CANHandler::generalConfig;
CANHandler* CANHandler::self;
Role CANHandler::role;

void CANHandler::handleIncomingMessagesTask() {
    bool handlingFinished = false;

    ESP_LOGD(CAN_HANDLER_LOG_TAG, "INCOMING - Started to handle incoming messages");
    while (!handlingFinished) {
        can_message_t message;

        ESP_LOGD(CAN_HANDLER_LOG_TAG, "INCOMING - Waiting for data from the CAN");
        int receiveResult = can_receive(&message, pdMS_TO_TICKS(CAN_RX_TIMEOUT_IN_MILLIS));

        if (receiveResult == ESP_OK) {
            ESP_LOGD(CAN_HANDLER_LOG_TAG, "INCOMING - New message received");
            if (self->role == Role::MASTER) {
                ESP_LOGD(MESSAGE_ATTACKER_LOG_TAG, "INCOMING - Checking if message needs to be attacked!");
                if (!self->messageAttacker->attackMessage(&message)) {
                    ESP_LOGE(CAN_HANDLER_LOG_TAG, "INCOMING - Error during attack\nTERMINATING");
                    handlingFinished = true;
                }
            } 

            self->uartHandler->sendByteArrayViaUART((uint8_t*)&message, CANHandler::usefulSizeOfACANMessageTypeVariable);
            ESP_LOGD(CAN_HANDLER_LOG_TAG, "INCOMING - Message sent via UART");
        } else {
            switch (receiveResult) {
                case ESP_ERR_TIMEOUT:
                    ESP_LOGD(CAN_HANDLER_LOG_TAG, "INCOMING - No new message received");
                    break;

                case ESP_ERR_INVALID_ARG:
                    ESP_LOGE(CAN_HANDLER_LOG_TAG, "INCOMING - Arguments are invalid\nTERMINATING");
                    handlingFinished = true;
                    break;

                case ESP_ERR_INVALID_STATE:
                    ESP_LOGE(CAN_HANDLER_LOG_TAG, "INCOMING - CAN driver is not installed\nTERMINATING");
                    handlingFinished = true;
                    break;

                default:
                    ESP_LOGE(CAN_HANDLER_LOG_TAG, "INCOMING - Unknown can_recieve error\nTERMINATING");
                    handlingFinished = true;
            }
        }
    }

    vTaskDelete(NULL);
    self->powerManager->haltCPU();
}

void CANHandler::handleOutgoingMessagesTask() {
    bool handlingFinished = false;
    ESP_LOGD(CAN_HANDLER_LOG_TAG, "OUTGOING - Started to handle outgoing messages");
    while (!handlingFinished) {
        can_message_t message;

        ESP_LOGD(CAN_HANDLER_LOG_TAG, "OUTGOING - Waiting for data from the UART");
        int sizeOfData;
        uint8_t* data = uartHandler->receiveByteArrayFromUART(sizeOfData);

        ESP_LOGD(CAN_HANDLER_LOG_TAG,
                 "OUTGOING - Copying the received data into a can_message_t struct, &message:%p, &data:%p, size:%d, data bellow.", &message, data,
                 sizeOfData);
        ESP_LOG_BUFFER_HEXDUMP(CAN_HANDLER_LOG_TAG, data, sizeOfData, ESP_LOG_DEBUG);
        memcpy(&message, data, sizeOfData);

        if (self->role == Role::MASTER) {
            ESP_LOGD(MESSAGE_ATTACKER_LOG_TAG, "OUTGOING - Checking if message needs to be attacked!");
            if (!self->messageAttacker->attackMessage(&message)) {
                ESP_LOGE(CAN_HANDLER_LOG_TAG, "OUTGOING - Error during attack\nTERMINATING");
                handlingFinished = true;
            }
        }

        int transmitResult = can_transmit(&message, pdMS_TO_TICKS(CAN_TX_TIMEOUT_IN_MILLIS));

        ESP_LOGD(CAN_HANDLER_LOG_TAG, "OUTGOING - Message sent!\nflags:0x%08x, ID:0x%08x, length:0x%02x, data:0x%02x%02x%02x%02x%02x%02x%02x%02x",
                 message.flags, message.identifier, message.data_length_code, message.data[0], message.data[1], message.data[2], message.data[3],
                 message.data[4], message.data[5], message.data[6], message.data[7]);

        if (transmitResult != ESP_OK) {
            switch (transmitResult) {
                case ESP_ERR_INVALID_ARG:
                    ESP_LOGE(CAN_HANDLER_LOG_TAG, "OUTGOING - Arguments are invalid\nTERMINATING");
                    handlingFinished = true;
                    break;

                case ESP_ERR_TIMEOUT:
                    ESP_LOGE(CAN_HANDLER_LOG_TAG, "OUTGOING - Bus too busy, a message has been discarded due to timeout");
                    break;

                case ESP_FAIL:
                    ESP_LOGE(CAN_HANDLER_LOG_TAG, "OUTGOING - TX buffer is disabled, and the bus is busy. The message has been discarded");
                    break;

                case ESP_ERR_INVALID_STATE:
                    ESP_LOGE(CAN_HANDLER_LOG_TAG, "OUTGOING - CAN driver is not installed or not running\nTERMINATING");
                    handlingFinished = true;
                    break;

                case ESP_ERR_NOT_SUPPORTED:
                    ESP_LOGE(CAN_HANDLER_LOG_TAG, "OUTGOING - CAN driver is in listen-only mode, and wanted to transmit\nTERMINATING");
                    handlingFinished = true;
                    break;

                default:
                    ESP_LOGE(CAN_HANDLER_LOG_TAG, "OUTGOING - Unknown can_recieve error\nTERMINATING");
                    handlingFinished = true;
            }
        }
    }

    vTaskDelete(NULL);
    self->powerManager->haltCPU();
}

//These wrappers hide the difference between c and object oriented c++
void vHandleIncomingMessagesTaskWrapper(void* parm) {
    static_cast<CANHandler*>(parm)->handleIncomingMessagesTask();
}

void vHandleOutgoingMessagesTaskWrapper(void* parm) {
    static_cast<CANHandler*>(parm)->handleOutgoingMessagesTask();
}

CANHandler::CANHandler(UARTHandler* uartHandler, PowerManager* powerManager, MessageAttacker* messageAttacker, Role _role) {
    self = this;
    
    role = _role;
    this->powerManager = powerManager;
    this->messageAttacker = messageAttacker;

    CANHandler::timingConfig = CAN_TIMING_CONFIG_1MBITS();
    CANHandler::filterConfig = CAN_FILTER_CONFIG_ACCEPT_ALL();
    CANHandler::generalConfig = CAN_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, CAN_MODE_NORMAL);

    

    this->uartHandler = uartHandler;

    can_message_t dummy;

    // In case the ESP-IDF version get's updated in platformio from the current 4.0, this calculation has to be reevaluated
    CANHandler::usefulSizeOfACANMessageTypeVariable =
        (sizeof(dummy.flags) + sizeof(dummy.identifier) + sizeof(dummy.data_length_code) + sizeof(dummy.data));

    if (can_driver_install(&CANHandler::generalConfig, &CANHandler::timingConfig, &CANHandler::filterConfig) == ESP_OK) {  // Install CAN driver
        ESP_LOGI(CAN_HANDLER_LOG_TAG, "Driver installed");
    } else {
        ESP_LOGE(CAN_HANDLER_LOG_TAG, "Failed to install driver");
        this->powerManager->haltCPU();
    }
}

void CANHandler::startHandlingCANMessages() {
    if (can_start() == ESP_OK) {  // Start CAN driver
        ESP_LOGI(CAN_HANDLER_LOG_TAG, "Starting handlers");

        //Creating the handler tasks. THESE WILL RUN SEPARATED FROM THIS OBJECT
        xTaskCreatePinnedToCore(vHandleIncomingMessagesTaskWrapper, "CAN_INPUT_TASK", CAN_RX_TASK_STACK_SIZE_IN_BYTES, this, CAN_RX_TASK_PRIORITY,
                                &incomingMessageHandlerTaskHandle, CAN_RX_TASK_CORE_NUMBER);
        xTaskCreatePinnedToCore(vHandleOutgoingMessagesTaskWrapper, "CAN_OUTPUT_TASK", CAN_TX_TASK_STACK_SIZE_IN_BYTES, this, CAN_TX_TASK_PRIORITY,
                                &outgoingMessageHandlerTaskHandle, CAN_TX_TASK_CORE_NUMBER);

        configASSERT(this->incomingMessageHandlerTaskHandle);
        configASSERT(this->outgoingMessageHandlerTaskHandle);

        ESP_LOGI(CAN_HANDLER_LOG_TAG, "Driver started");
    } else {
        ESP_LOGE(CAN_HANDLER_LOG_TAG, "Failed to start driver");
        this->powerManager->haltCPU();
    }
}

void CANHandler::setNewBitrate(int bitrate) {
    ESP_LOGD(CAN_HANDLER_LOG_TAG, "Uninstalling CAN driver for the bitrate chaning procedure!");
    can_driver_uninstall();

    switch(bitrate){
        case 25000:
            CANHandler::timingConfig = CAN_TIMING_CONFIG_25KBITS();
            ESP_LOGD(CAN_HANDLER_LOG_TAG, "New bitrate set to 25kbps");
            break;

        case 50000:
            CANHandler::timingConfig = CAN_TIMING_CONFIG_50KBITS();
            ESP_LOGD(CAN_HANDLER_LOG_TAG, "New bitrate set to 50kbps");
            break;

        case 100000:
            CANHandler::timingConfig = CAN_TIMING_CONFIG_100KBITS();
            ESP_LOGD(CAN_HANDLER_LOG_TAG, "New bitrate set to 100kbps");
            break;

        case 125000:
            CANHandler::timingConfig = CAN_TIMING_CONFIG_125KBITS();
            ESP_LOGD(CAN_HANDLER_LOG_TAG, "New bitrate set to 125kbps");
            break;

        case 250000:
            CANHandler::timingConfig = CAN_TIMING_CONFIG_250KBITS();
            ESP_LOGD(CAN_HANDLER_LOG_TAG, "New bitrate set to 250kbps");
            break;

        case 500000:
            CANHandler::timingConfig = CAN_TIMING_CONFIG_500KBITS();
            ESP_LOGD(CAN_HANDLER_LOG_TAG, "New bitrate set to 500kbps");
            break;

        case 800000:
            CANHandler::timingConfig = CAN_TIMING_CONFIG_800KBITS();
            ESP_LOGD(CAN_HANDLER_LOG_TAG, "New bitrate set to 800kbps");
            break;

        case 1000000:
            CANHandler::timingConfig = CAN_TIMING_CONFIG_1MBITS();
            ESP_LOGD(CAN_HANDLER_LOG_TAG, "New bitrate set to 1Mbps");
            break;

        default:
            ESP_LOGE(CAN_HANDLER_LOG_TAG, "Failed to set new baudrate, unknown bitrate was given!");
            this->powerManager->haltCPU();
    }

    CANHandler::filterConfig = CAN_FILTER_CONFIG_ACCEPT_ALL();
    CANHandler::generalConfig = CAN_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, CAN_MODE_NORMAL);

    if (can_driver_install(&CANHandler::generalConfig, &CANHandler::timingConfig, &CANHandler::filterConfig) == ESP_OK) {  // reinstall CAN driver
        ESP_LOGD(CAN_HANDLER_LOG_TAG, "CAN driver successfully reinstalled!");
    } else {
        ESP_LOGE(CAN_HANDLER_LOG_TAG, "Failed to start CAN driver with the new bitrate!");
        this->powerManager->haltCPU();
    }
}