#ifndef MESSAGEATTACKER_H
#define MESSAGEATTACKER_H

#include "driver/can.h"

#include "AttackConfig.h"
#include "AttackType.h"

/**
 * This class can attack a given message based on the attack config and the message contents.
 */
class MessageAttacker {
   private:
    AttackConfig* attackConfig;
    static AttackType attackType;
    static uint32_t idToBeAttacked;
    static uint8_t constValue;
    static uint8_t deltaValue;
    static uint8_t upperLimiterValue;
    static uint8_t lowerLimiterValue;
    static uint8_t counter;
    static int offset;
    static int attackLength;

   public:
    MessageAttacker(AttackConfig* attackConfig);
    void updateAttackConfig();
    bool attackMessage(can_message_t* canMessage);
};

#endif