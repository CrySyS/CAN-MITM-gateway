#ifndef ATTACKTYPE_H
#define ATTACKTYPE_H

/**
 * This enum contains the implemented attack types. For details, please check out the wiki:
 * https://git.crysys.hu/student-projects/2019-Andris-FerencziCsongor-CANHacking/-/wikis/home
 */
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