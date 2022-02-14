#include "AttackConfig.h"

#include <stdint.h>

#include "AttackType.h"

AttackConfig::AttackConfig() {
    this->bitrate = 0;
    this->attackType = AttackType::PASSTHROUGH;
    this->idToBeAttacked = 0;
    this->offset = 0;
    this->attackLength = 0;
    this->constValue = 0;
    this->deltaValue = 0;
}

void AttackConfig::setNewAttackConfig(int bitrate, AttackType attackType, uint32_t idToBeAttacked, int offset, int attackLength, uint8_t constValue, uint8_t deltaValue){
    this->bitrate = bitrate;
    this->attackType = attackType;
    this->idToBeAttacked = idToBeAttacked;
    this->offset = offset;
    this->attackLength = attackLength;
    this->constValue = constValue;
    this->deltaValue = deltaValue;
}

int AttackConfig::getBitrate() {
    return this->bitrate;
}

AttackType AttackConfig::getAttackType() {
    return this->attackType;
}

uint32_t AttackConfig::getIDToBeAttacked() {
    return this->idToBeAttacked;
}

int AttackConfig::getOffset() {
    return this->offset;
}

int AttackConfig::getAttackLength() {
    return this->attackLength;
}

uint8_t AttackConfig::getConstValue() {
    return this->constValue;
}

uint8_t AttackConfig::getDeltaValue() {
    return this->deltaValue;
}