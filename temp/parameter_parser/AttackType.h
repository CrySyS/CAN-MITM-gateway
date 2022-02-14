
#ifndef ATTACKTYPE_H
#define ATTACKTYPE_H

enum class AttackType {
    PASSTHROUGH,
    REPLACE_DATA_WITH_CONSTANT_VALUES,
    REPLACE_DATA_WITH_RANDOM_VALUES,
    ADD_DELTA_VALUE_TO_THE_DATA,
    SUBTRACT_DELTA_VALUE_FROM_THE_DATA,
    INCREASE_DATA_UNTIL_MAX_VALUE,
    DECREASE_DATA_UNTIL_MIN_VALUE,
    REPLACE_DATA_WITH_INCREASING_COUNTER,
    REPLACE_DATA_WITH_DECREASING_COUNTER
};

#endif

/*

const: the original data value is replaced by the given attack data.
random: the original data value is replaced by a new random value in every selected message.
delta: the given attack data is added to the original data value.
add_incr: an increasing value (per selected message) is added to the original data value.
add_decr: an increasing value (per selected message) is substracted from the original value.
change_incr: the original data value is replaced by an increasing value (per selected message)
change_decr: the original data value is replaced by a decreasing value (per selected message)

*/
