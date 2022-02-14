#ifndef CANHANDLER_H
#define CANHANDLER_H

#include "driver/can.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "MessageAttacker.h"
#include "PowerManager.h"
#include "Role.h"
#include "UARTHandler.h"

/**
 * This class handles the CAN interface. It can setup the device, send and receive messages.
 */
class CANHandler {
   private:
    static can_timing_config_t timingConfig;
    static can_filter_config_t filterConfig;
    static can_general_config_t generalConfig;

    UARTHandler* uartHandler;
    PowerManager* powerManager;
    MessageAttacker* messageAttacker;
    static Role role;

    TaskHandle_t incomingMessageHandlerTaskHandle;
    TaskHandle_t outgoingMessageHandlerTaskHandle;

    static int usefulSizeOfACANMessageTypeVariable;
    static CANHandler* self;

   public:
    CANHandler(UARTHandler* uartHandler, PowerManager* powerManager, MessageAttacker* messageAttacker, Role role);
    void handleIncomingMessagesTask();
    void handleOutgoingMessagesTask();
    void startHandlingCANMessages();

    void setNewBitrate(int bitrate);
};

#endif