#ifndef UARTHANDLER_H
#define UARTHANDLER_H

#include "driver/uart.h"

#include "PowerManager.h"
#include "Role.h"

/**
 * This class can send and receive messages through UART
 */
class UARTHandler {
   private:
    uart_config_t driverConfig;
    static uint8_t* rxBuffer;
    static QueueHandle_t rxQueue;
    static uart_event_t event;

    Role role;
    PowerManager* powerManager;

   public:
    UARTHandler(PowerManager* powerManager, Role role);
    bool testConnection();
    void emptyBuffer();

    void sendByteArrayViaUART(const uint8_t* data, int len);
    uint8_t* receiveByteArrayFromUART(int& sizeOfData);

    void sendNewCANBitrateToSlave(int bitrate);
    bool receiveNewCANBitrateSettings(int& bitrate);
};

#endif