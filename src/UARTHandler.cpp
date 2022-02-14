#include "UARTHandler.h"

#include <stdio.h>
#include <string.h>
#include <string>


#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "PowerManager.h"
#include "Role.h"
#include "defines.h"

uint8_t* UARTHandler::rxBuffer;
QueueHandle_t UARTHandler::rxQueue;
uart_event_t UARTHandler::event;

UARTHandler::UARTHandler(PowerManager* powerManager, Role role) {
    this->role = role;
    this->powerManager = powerManager;

    this->driverConfig = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE /*,
        .use_ref_tick = true*/ //Don't worry about the warnings, we don't use these functionalities
    };

    UARTHandler::rxBuffer = (uint8_t*)malloc(UART_RX_BUFFER_SIZE * sizeof(uint8_t));

    if ((uart_param_config(UART_PORT, &this->driverConfig) == ESP_OK) &&
        (uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) == ESP_OK) &&
        (uart_driver_install(UART_PORT, UART_RX_BUFFER_SIZE, UART_TX_BUFFER_SIZE, UART_QUEUE_SIZE, &UARTHandler::rxQueue, 0) == ESP_OK)) {
        ESP_LOGI(UART_HANDLER_LOG_TAG, "Driver installed");
    } else {
        ESP_LOGE(UART_HANDLER_LOG_TAG, "Failed to install driver");
        this->powerManager->haltCPU();
    }

    
    if (this->testConnection()) {
        ESP_LOGI(MAIN_LOG_TAG, "Passed UART connection test");
    } else {
        ESP_LOGE(MAIN_LOG_TAG, "Failed to test UART connection, powering down!");
        this->powerManager->haltCPU();
    }
}

/**
 * This connection testing function does a "TCP handshake" like communication through the UART, in order to check if everything is okay.
 */
bool UARTHandler::testConnection() {
    ESP_LOGI(UART_HANDLER_LOG_TAG, "Testing connection!");
    int startTime = esp_timer_get_time();

    int phase = this->role == Role::MASTER ? 0 : 1;
    char phases[3][9] = {"SYN\0", "SYN,ACK\0", "ACK\0"}; //Master sends SYN, slave responds with SYN,ACK, master responds with ACK.

    while (esp_timer_get_time() < startTime + UART_CONNECTION_CHECK_TIMEOUT_IN_MICROS) {
        if (phase == 0) { //This part is for the Master
            int length = 0;
            ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_PORT, (size_t*)&length));
            if (length == 0) { //if no data arrives
                uart_write_bytes(UART_PORT, phases[phase], strlen(phases[phase]) + 1); //sending SYN

                ESP_LOGD(UART_HANDLER_LOG_TAG, "Waiting for the slave, sent the following text: %s", phases[phase]);
                vTaskDelay(UART_CONNECTION_CHECK_SLAVE_NOT_ANSWERS_DELAY / portTICK_PERIOD_MS);
            } else {  //data arrived
                phase = 2;
                uint8_t* connectionCheckRXBuffer = (uint8_t*)malloc(length * sizeof(uint8_t));

                uart_read_bytes(UART_PORT, connectionCheckRXBuffer, length, portMAX_DELAY); //reading the data
                ESP_LOGD(UART_HANDLER_LOG_TAG, "Received %s, expected: %s", connectionCheckRXBuffer, phases[phase - 1]);
                if (strcmp((char*)connectionCheckRXBuffer, phases[phase - 1]) == 0) { //If the data equals to SYN,ACK
                    uart_write_bytes(UART_PORT, phases[phase], strlen(phases[phase]) + 1);

                    ESP_LOGD(UART_HANDLER_LOG_TAG, "Sent the following text: %s", phases[phase]);
                    ESP_LOGD(UART_HANDLER_LOG_TAG, "Connection test passed");
                    free(connectionCheckRXBuffer);
                    return true;
                } else {
                    ESP_LOGE(UART_HANDLER_LOG_TAG, "Connection test failed! Received %s with the length of %d in phase %d\nTERMINATING",
                             connectionCheckRXBuffer, length, phase);
                    ESP_LOG_BUFFER_HEXDUMP(UART_HANDLER_LOG_TAG, connectionCheckRXBuffer, length, ESP_LOG_ERROR);
                    free(connectionCheckRXBuffer);
                    return false;
                }
            }
        } else { //This is the slave part
            int length = 0;
            ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_PORT, (size_t*)&length));

            if (length == 0) {
                ESP_LOGD(UART_HANDLER_LOG_TAG, "No message from the master, delaying...");
                vTaskDelay(UART_CONNECTION_CHECK_NO_NEW_MESSAGE_FROM_MASTER / portTICK_PERIOD_MS);
            } else {
                uint8_t* connectionCheckRXBuffer = (uint8_t*)malloc(length * sizeof(uint8_t));

                uart_read_bytes(UART_PORT, connectionCheckRXBuffer, length, portMAX_DELAY);
                ESP_LOGD(UART_HANDLER_LOG_TAG, "Received %s, expected: %s", connectionCheckRXBuffer, phases[phase - 1]);
                if (strcmp((char*)connectionCheckRXBuffer, phases[phase - 1]) == 0) { //if we got SYN
                    if (phase == 1) {
                        uart_write_bytes(UART_PORT, phases[phase], strlen(phases[phase]) + 1); //we send SYN,ACK

                        ESP_LOGD(UART_HANDLER_LOG_TAG, "Sent the following text: %s", phases[phase]);
                        phase = 3;
                    } else if (phase == 3) {
                        ESP_LOGD(UART_HANDLER_LOG_TAG, "Connection test passed");
                        free(connectionCheckRXBuffer);
                        return true;
                    }
                } else {
                    ESP_LOGE(UART_HANDLER_LOG_TAG, "Connection test failed! Received %s with the length of %d in phase %d\nTERMINATING",
                             connectionCheckRXBuffer, length, phase);
                    ESP_LOG_BUFFER_HEXDUMP(UART_HANDLER_LOG_TAG, connectionCheckRXBuffer, length, ESP_LOG_ERROR);
                    free(connectionCheckRXBuffer);
                    return false;
                }
            }
        }
    }

    ESP_LOGE(UART_HANDLER_LOG_TAG, "Connection test failed! Timed out!\nTERMINATING");
    return false;
}

void UARTHandler::emptyBuffer() {
    xQueueReset(UARTHandler::rxQueue);
    uart_flush(UART_PORT);
}

void UARTHandler::sendByteArrayViaUART(const uint8_t* data, int len) {
    ESP_LOGD(UART_HANDLER_LOG_TAG, "TX - Sending %d bytes via UART", len);
    int bytesSent = uart_write_bytes(UART_PORT, (char*)data, len);

    if (bytesSent == -1) { //if we were not able to send the data
        ESP_LOGE(UART_HANDLER_LOG_TAG, "TX - Parameter error during TX! len: %d, data bellow.", len);
        ESP_LOG_BUFFER_HEXDUMP(UART_HANDLER_LOG_TAG, data, len, ESP_LOG_ERROR);
    } else if (bytesSent != len) {
        ESP_LOGE(UART_HANDLER_LOG_TAG, "TX - Couldn't send all of the bytes! Wanted to send %d but only sent %d instead!", len, bytesSent);
    }
}

uint8_t* UARTHandler::receiveByteArrayFromUART(int& sizeOfData) {
    ESP_LOGD(UART_HANDLER_LOG_TAG, "RX - Filling the RX buffer with zeros");
    memset(UARTHandler::rxBuffer, 0x00, sizeof(uint8_t) * UART_RX_BUFFER_SIZE);

    ESP_LOGD(UART_HANDLER_LOG_TAG, "RX - Waiting for UART event to arrive");
    bool dataArrived = false;
    while (!dataArrived) {
        ESP_LOGV(UART_HANDLER_LOG_TAG, "rxBufferPointer: %p, rxQueuePointer: %p, event:%p", &rxBuffer, &rxQueue, &event);
        if (xQueueReceive(UARTHandler::rxQueue, (void*)&event, portMAX_DELAY) == pdTRUE) {
            ESP_LOGD(UART_HANDLER_LOG_TAG, "RX - New event arrived, type: %d, size: %d", event.type, event.size);
            if (event.type == UART_DATA) {
                uart_read_bytes(UART_PORT, UARTHandler::rxBuffer, event.size, portMAX_DELAY);
                sizeOfData = event.size;
                // ESP_LOG_BUFFER_HEXDUMP(UART_HANDLER_LOG_TAG, UARTHandler::rxBuffer, event.size, ESP_LOG_DEBUG); //dump the received data
                dataArrived = true; 
                return UARTHandler::rxBuffer;
            } else {
                ESP_LOGE(UART_HANDLER_LOG_TAG, "RX - Not handled UART event type: %d", event.type);
            }
        }  // else, timeout
    }

    return NULL;  // This point is never reached, but needed for the compiler
}

void UARTHandler::sendNewCANBitrateToSlave(int bitrate){
    ESP_LOGD(UART_HANDLER_LOG_TAG, "Sending new CAN bus bitrate to the slave");
    const char* bitrateString = std::to_string(bitrate).c_str();
    int len = strlen(bitrateString);

    int bytesSent = uart_write_bytes(UART_PORT, bitrateString, len);

    if (bytesSent == -1 || bytesSent != len) {  // if we were not able to send the data
        ESP_LOGE(UART_HANDLER_LOG_TAG, "Error during sending new CAN bitrate! Only sent %d bytes instead of %d bytes", bytesSent, len);
        this->powerManager->haltCPU();
    }
    ESP_LOGD(UART_HANDLER_LOG_TAG, "Data sent successfully");
}

bool UARTHandler::receiveNewCANBitrateSettings(int& bitrate){
    int length = 0;
    ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_PORT, (size_t*)&length));

    if(length > 0){
        ESP_LOGD(UART_HANDLER_LOG_TAG, "Data waiting in UART buffer, this should be the CAN Bitrate");
        uint8_t* newCANBitrateConfigRXBuffer = (uint8_t*)malloc(length * sizeof(uint8_t)); 
        uart_read_bytes(UART_PORT, newCANBitrateConfigRXBuffer, length, portMAX_DELAY);

        ESP_LOGD(UART_HANDLER_LOG_TAG, "%d bytes has been received, parsing into bitrate.", length);
        bitrate = strtol((char*)newCANBitrateConfigRXBuffer, NULL, 10);

        switch (bitrate) {
            case 25000:
            case 50000:
            case 100000:
            case 125000:
            case 250000:
            case 500000:
            case 800000:
            case 1000000:
                ESP_LOGD(UART_HANDLER_LOG_TAG, "Parsed bitrate: %d", bitrate);
                return true;
                break;

            default:
                ESP_LOGE(UART_HANDLER_LOG_TAG, "Error during receiving new CAN bitrate! Received: %s Parsed bitrate: %d not valid!", (char*)newCANBitrateConfigRXBuffer, bitrate);
                this->powerManager->haltCPU();
        }
    }
    return false;
}