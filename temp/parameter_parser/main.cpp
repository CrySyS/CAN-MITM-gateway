#include <iostream>
#include <string>
#include <regex>

#include "AttackType.h"
int main()
{
    std::string requestStrings[] = {"attackType=PASSTHROUGH",
                                   "attackType=PASSTHROUGH&id=&offset=&attackLength=&byteValue=",
                                   "id=11ac&offset=2&attackLength=2&attackType=REPLACE_DATA_WITH_CONSTANT_VALUES&byteValue=0f",
                                   "attackType=REPLACE_DATA_WITH_RANDOM_VALUES&attackLength=1&id=aa1&offset=2&byteValue=",
                                   "attackType=SUBTRACT_DELTA_VALUE_FROM_THE_DATA&id=aa2&offset=2&attackLength=1&byteValue=aa",
                                   "attackType=AAAAAAAAAAA&id=&offset=2&attackLength=2&byteValue=0f",
                                   "attackType=REPLACE_DATA_WITH_CONSTANT_VALUES&id=11ac&offset&attackLength=2&byteValue=0f",
                                   "attackType=REPLACE_DATA_WITH_CONSTANT_VALUES&id=11ac&offset=2&attackLength=2&byteValue=",
                                   "id=11ac&offset=2&attackLength=2&byteValue=",
                                   "id=11ac&offset=2&attackLength=2&byteValue=&",
                                   "id=&=2&attackLength=2&byteValue=&=&&&==",
                                   "id=1a2b3c4f5&offset=a&attackLength=2&byteValue=",
                                   "id=11ac&offset=9&attackLength=2&byteValue=",
                                   "id=11ac&offset=7&attackLength=2&byteValue=",
                                   "byteValue=0f",
                                   ""};
    std::string errorString = "";
    std::string parameters[5];

    for(int i = 0; i < sizeof(requestStrings)/sizeof(*requestStrings); i++){
        std::string requestString = requestStrings[i];

        AttackType attackType = AttackType::PASSTHROUGH;
        uint32_t idToBeAttacked = 0;
        int offset = 0;
        int attackLength = 0;
        uint8_t byteValue = 0;

        bool attackTypeFound = false;
        bool idFound = false;
        bool attackOffsetFound = false;
        bool attackLengthFound = false;
        bool byteValueFound = false;

        std::cout<<"Request: "<<requestString<<std::endl<< std::endl;

        if(requestString.length() > 0){ //If the request has characters
            if(requestString.compare("attackType=PASSTHROUGH") == 0){ //this is the only valid request that has no & symbol
                attackType = AttackType::PASSTHROUGH;
                attackTypeFound = true;

                std::cout << "Parsed parameters:" << std::endl;
                std::cout << "AttackType: " << static_cast<std::underlying_type<AttackType>::type>(attackType) << std::endl;

            } else if(requestString.find("&") != std::string::npos){ //if we've found a & symbol
                std::string restOfTheRequestString = requestString;
                int foundParameterCounter = 0;
                while(restOfTheRequestString.find("&") != std::string::npos){
                    int firstAndSymbolPosition = restOfTheRequestString.find("&");
                    std::string foundParameterCandidate = restOfTheRequestString.substr(0, firstAndSymbolPosition); //we get the first substring until the & symbol

                    int equalSymbolPosition = foundParameterCandidate.find("=");
                    if(equalSymbolPosition != std::string::npos && equalSymbolPosition != 0 && foundParameterCandidate.length() > equalSymbolPosition + 1){ //if there is an equal symbol in the candidate, and there is data before and after it
                        parameters[foundParameterCounter] = foundParameterCandidate; //save the parameter
                        foundParameterCounter++;
                    } //otherwise we skip this parameter, since there was no data set

                    restOfTheRequestString = restOfTheRequestString.substr(firstAndSymbolPosition+1); //cut off the head including the & symbol
                }

                //Handle the last parameter, if there is one
                int equalSymbolPosition = restOfTheRequestString.find("=");
                if(equalSymbolPosition != std::string::npos && equalSymbolPosition != 0 && restOfTheRequestString.length() > equalSymbolPosition + 1){ //if there is an equal symbol in the rest of the request and there is data before and after it (this excludes ending with & symbol)
                    parameters[foundParameterCounter] = restOfTheRequestString; //save the last parameter
                    foundParameterCounter++;
                }


                // parse variables
                for(int i = 0; i < foundParameterCounter; i++){
                    std::cout << "Raw parameter string: "<<parameters[i] << std::endl;

                    //At this point we know that every parameter entry has data in X=Y format
                    int equalSymbolPosition = parameters[i].find("=");
                    std::string key = parameters[i].substr(0, equalSymbolPosition);
                    std::string value = parameters[i].substr(equalSymbolPosition + 1);

                    if(key.compare("attackType") == 0){
                        if(value.compare("PASSTHROUGH") == 0){
                            attackType = AttackType::PASSTHROUGH;
                            attackTypeFound = true;
                        } else if(value.compare("REPLACE_DATA_WITH_CONSTANT_VALUES") == 0){
                            attackType = AttackType::REPLACE_DATA_WITH_CONSTANT_VALUES;
                            attackTypeFound = true;
                        } else if(value.compare("REPLACE_DATA_WITH_RANDOM_VALUES") == 0){
                            attackType = AttackType::REPLACE_DATA_WITH_RANDOM_VALUES;
                            attackTypeFound = true;
                        } else if(value.compare("ADD_DELTA_VALUE_TO_THE_DATA") == 0){
                            attackType = AttackType::ADD_DELTA_VALUE_TO_THE_DATA;
                            attackTypeFound = true;
                        } else if(value.compare("SUBTRACT_DELTA_VALUE_FROM_THE_DATA") == 0){
                            attackType = AttackType::SUBTRACT_DELTA_VALUE_FROM_THE_DATA;
                            attackTypeFound = true;
                        } else if(value.compare("INCREASE_DATA_UNTIL_MAX_VALUE") == 0){
                            attackType = AttackType::INCREASE_DATA_UNTIL_MAX_VALUE;
                            attackTypeFound = true;
                        } else if(value.compare("DECREASE_DATA_UNTIL_MIN_VALUE") == 0){
                            attackType = AttackType::DECREASE_DATA_UNTIL_MIN_VALUE;
                            attackTypeFound = true;
                        } else if(value.compare("REPLACE_DATA_WITH_INCREASING_COUNTER") == 0){
                            attackType = AttackType::REPLACE_DATA_WITH_INCREASING_COUNTER;
                            attackTypeFound = true;
                        } else if(value.compare("REPLACE_DATA_WITH_DECREASING_COUNTER") == 0){
                            attackType = AttackType::REPLACE_DATA_WITH_DECREASING_COUNTER;
                            attackTypeFound = true;
                        } else {
                            //Wrong attackType
                            std::cout << "Wrong attack type set!" << std::endl;
                        }

                    } else if(key.compare("id") == 0){
                        if(regex_match(value, std::regex("^[0-9a-fA-F]{1,8}$"))){
                            idToBeAttacked = strtoul(value.c_str(), NULL, 16);
                            idFound = true;
                        } else {
                            //bad pameter value
                            std::cout << "Bad ID value set!" << std::endl;
                        }

                    } else if(key.compare("offset") == 0){
                        if(regex_match(value, std::regex("^[0-7]$"))){
                            offset = strtoul(value.c_str(), NULL, 10);
                            attackOffsetFound = true;
                        } else {
                            //bad pameter value
                            std::cout << "Bad offset value set!" << std::endl;
                        }

                    } else if(key.compare("attackLength") == 0){
                        if(regex_match(value, std::regex("^[1-8]$"))){
                            attackLength = strtoul(value.c_str(), NULL, 10);
                            attackLengthFound = true;
                        } else {
                            //bad pameter value
                            std::cout << "Bad attackLength set!" << std::endl;
                        }

                    } else if(key.compare("byteValue") == 0){
                        if(regex_match(value, std::regex("^[0-9a-fA-F]{1,2}$"))){
                            byteValue = strtoul(value.c_str(), NULL, 16);
                            byteValueFound = true;
                        } else {
                            //bad pameter value
                            std::cout << "Bad byteValue set!" << std::endl;
                        }

                    } else {
                        //Bad parameter type
                        std::cout << "Bad parameter type set!" << std::endl;
                    }
                }

                // Validate attack length and offset sum
                if(attackOffsetFound && attackLengthFound){
                    if((offset + attackLength) > 8){
                        //Bad parameter sum
                        std::cout << "Sum of attack length and offset > 8!" << std::endl;
                    }
                }

                std::cout << std::endl << "Parsed parameters:" << std::endl;
                std::stringstream hexStringStream;
                hexStringStream << std::hex;

                std::cout << "AttackType: " << (attackTypeFound ? std::to_string(static_cast<std::underlying_type<AttackType>::type>(attackType)) : "NOT SET") << std::endl;

                hexStringStream << idToBeAttacked;
                std::cout << "ID: " << (idFound ? hexStringStream.str() : "NOT SET") << std::endl;

                std::cout << "Offset: " << (attackOffsetFound ? std::to_string(offset) : "NOT SET") << std::endl;

                std::cout << "AttackLength: " << (attackLengthFound ? std::to_string(attackLength) : "NOT SET") << std::endl;

                hexStringStream.str("");
                hexStringStream << (short) byteValue;
                std::cout << "ByteValue: " << (byteValueFound ? hexStringStream.str() : "NOT SET") << std::endl << std::endl;



                //Check if the correct parameters are set
                if(attackTypeFound){
                    switch(attackType){
                        case AttackType::PASSTHROUGH:
                            //No parameter needed
                            break;

                        case AttackType::REPLACE_DATA_WITH_CONSTANT_VALUES:
                        case AttackType::ADD_DELTA_VALUE_TO_THE_DATA:
                        case AttackType::SUBTRACT_DELTA_VALUE_FROM_THE_DATA:
                            if(!idFound || !attackOffsetFound || !attackLengthFound || !byteValueFound){
                                std::cout << "Valid id, offset, attackLength, byteValue required. Format example: attackType=REPLACE_DATA_WITH_CONSTANT_VALUES&id=0a1B2c3D&offset=1&attackLength=2&byteValue=a3" << std::endl;
                            }
                            break;

                        case AttackType::REPLACE_DATA_WITH_RANDOM_VALUES:
                        case AttackType::INCREASE_DATA_UNTIL_MAX_VALUE:
                        case AttackType::DECREASE_DATA_UNTIL_MIN_VALUE:
                        case AttackType::REPLACE_DATA_WITH_INCREASING_COUNTER:
                        case AttackType::REPLACE_DATA_WITH_DECREASING_COUNTER:
                            if(!idFound || !attackOffsetFound || !attackLengthFound){
                                std::cout << "Valid id, offset, attackLength required. Format example: attackType=REPLACE_DATA_WITH_CONSTANT_VALUES&id=0a1B2c3D&offset=1&attackLength=2&byteValue=a3" << std::endl;
                            }
                            break;

                        default:
                            // Internal error, should never happen
                            std::cout << "Internal error, unexpected AttackTypeEnum" << std::endl;
                    }

                    // At this point, input is parsed into parameters, values have been checked, and
                    // every parameter required by the attack is set.

                    // Configuration successful!
                    std::cout << "Configuration successful!" << std::endl;
                } else {
                    std::cout << "No attack type was set!" << std::endl;
                }

            } else {
                //Invalid data
                std::cout << "Invalid data! Format example: attackType=REPLACE_DATA_WITH_CONSTANT_VALUES&id=0a1B2c3D&offset=1&attackLength=2&byteValue=a3" << std::endl;
            }

        } else {
            //No data set in request
            std::cout << "No data set in request" << std::endl;
        }
        std::cout<< std::endl<< std::endl<< std::endl << std::endl << std::endl;
    }

    return 0;
}
