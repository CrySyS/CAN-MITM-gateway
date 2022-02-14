#ifndef ATTACKCONFIG_H
#define ATTACKCONFIG_H

#include <stdint.h>

#include "AttackType.h"

/**
 * This class stores the parameters of the attack we want to execute.
 */ 
class AttackConfig {
   private:
    int bitrate;
    AttackType attackType;
    uint32_t idToBeAttacked;
    int offset;
    int attackLength;
    uint8_t constValue;
    uint8_t deltaValue;

   public:
    AttackConfig();
    void setNewAttackConfig(int bitrate, AttackType attackType, uint32_t idToBeAttacked, int offset, int attackLength, uint8_t constValue, uint8_t deltaValue);
    int getBitrate();
    AttackType getAttackType();
    uint32_t getIDToBeAttacked();
    int getOffset();
    int getAttackLength();
    uint8_t getConstValue();
    uint8_t getDeltaValue();
};

#endif