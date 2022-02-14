#include "WebServerHandler.h"

#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_eth.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"

#include "defines.h"

#include <algorithm>
#include <regex>

//These variables can be used to locate a file in the progmem.
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

extern const uint8_t main_css_start[]   asm("_binary_main_css_start");
extern const uint8_t main_css_end[]     asm("_binary_main_css_end");

//These descriptors will be registered, thus these urls will be served
httpd_uri_t WebServerHandler::getRootURIDescriptor = {.uri = "/", .method = HTTP_GET, .handler = WebServerHandler::getRequestHandler, .user_ctx = NULL};
httpd_uri_t WebServerHandler::getIndexURIDescriptor = {.uri = "/index.html", .method = HTTP_GET, .handler = WebServerHandler::getRequestHandler, .user_ctx = NULL};

httpd_uri_t WebServerHandler::getMainCssURIDescriptor = {.uri = "/css/main.css", .method = HTTP_GET, .handler = WebServerHandler::getRequestHandler, .user_ctx = NULL};

httpd_uri_t WebServerHandler::postURIDescriptor = {.uri = "/config",
                                                   .method = HTTP_POST,
                                                   .handler = WebServerHandler::postRequestHandler,
                                                   .user_ctx = NULL};

AttackConfig* WebServerHandler::attackConfig;
bool WebServerHandler::configReceived;

/**
 * This is a chunky function. All it does is parse the config request, validate every parameter and its value, and set the AttackConfig if everything checks out.
 */
bool WebServerHandler::parseRequestContentIntoAttackConfig(const char* requestContent, std::string& successString, std::string& errorString) {
    std::string requestString = std::string(requestContent);

    int bitrate = 0;
    AttackType attackType = AttackType::PASSTHROUGH;
    uint32_t idToBeAttacked = 0;
    int offset = 0;
    int attackLength = 0;
    uint8_t byteValue = 0;

    std::string parameters[NUMBER_OF_MAXIMUM_PARAMETERS];

    bool bitrateFound = false;
    bool attackTypeFound = false;
    bool idFound = false;
    bool attackOffsetFound = false;
    bool attackLengthFound = false;
    bool byteValueFound = false;

    if (requestString.length() > 0) {                                // If the request has characters
        if (requestString.find("&") != std::string::npos) {  // if we've found a & symbol, thus there could be multiple paramters
            std::string restOfTheRequestString = requestString;
            int foundParameterCounter = 0;
            while (restOfTheRequestString.find("&") != std::string::npos) {
                int firstAndSymbolPosition = restOfTheRequestString.find("&");
                std::string foundParameterCandidate = restOfTheRequestString.substr(0, firstAndSymbolPosition);  // we get the first substring until the & symbol

                int equalSymbolPosition = foundParameterCandidate.find("=");
                if (equalSymbolPosition != std::string::npos && equalSymbolPosition != 0 && foundParameterCandidate.length() > equalSymbolPosition + 1) {   // if there is an equal symbol in the candidate, 
                                                                                                                                                            //and there is data before and after it
                    ESP_LOGD(WEBSERVER_LOG_TAG, "Found parameter candidate: %s", foundParameterCandidate.c_str());
                    if (foundParameterCounter + 1 > NUMBER_OF_MAXIMUM_PARAMETERS){
                        errorString = "Number of maximum parameters (";
                        errorString += std::to_string(NUMBER_OF_MAXIMUM_PARAMETERS);
                        errorString += ") has been exceeded!";
                        return false;
                    } else {
                        parameters[foundParameterCounter] = foundParameterCandidate;  // save the parameter
                        foundParameterCounter++;
                    }

                } else {  // otherwise we skip this parameter, since there was no data set on at least one side
                    ESP_LOGD(WEBSERVER_LOG_TAG, "Parameter candidate skipped: %s", foundParameterCandidate.c_str());
                }

                restOfTheRequestString = restOfTheRequestString.substr(firstAndSymbolPosition + 1);  // cut off the saved or skipped parameter in the front, including the & symbol
            }

            // Handle the last parameter, if there is one
            int equalSymbolPosition = restOfTheRequestString.find("="); //we search the last parameter by the = symbol
            if (equalSymbolPosition != std::string::npos && equalSymbolPosition != 0 &&
                restOfTheRequestString.length() > equalSymbolPosition + 1) {  // if there is an equal symbol in the rest of the request and there is
                                                                              // data before and after it
                ESP_LOGD(WEBSERVER_LOG_TAG, "Found last parameter candidate: %s", restOfTheRequestString.c_str());
                if (foundParameterCounter + 1 > NUMBER_OF_MAXIMUM_PARAMETERS) {
                    errorString = "Number of maximum parameters (";
                    errorString += std::to_string(NUMBER_OF_MAXIMUM_PARAMETERS);
                    errorString += ") has been exceeded!";
                    return false;
                } else {
                    parameters[foundParameterCounter] = restOfTheRequestString;  // save the last parameter
                    foundParameterCounter++;
                }

            } else {
                ESP_LOGD(WEBSERVER_LOG_TAG, "Rest of the request is not interesting: %s", restOfTheRequestString.c_str());
            }

            std::string parseErrorString = ""; //This variable will be used to set multiple error messages in one variable.
            // parse variables
            for (int i = 0; i < foundParameterCounter; i++) {
                ESP_LOGD(WEBSERVER_LOG_TAG, "Processing parameter string: %s", parameters[i].c_str());

                // At this point we know that every parameter entry has data in X=Y format
                int equalSymbolPosition = parameters[i].find("=");
                std::string key = parameters[i].substr(0, equalSymbolPosition);
                std::string value = parameters[i].substr(equalSymbolPosition + 1);

                if (key.compare("bitrate") == 0) {
                    if (value.compare("25kbps") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "25kbps bitrate detected");
                        bitrate = 25000;
                        bitrateFound = true;
                    } else if (value.compare("50kbps") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "50kbps bitrate detected");
                        bitrate = 50000;
                        bitrateFound = true;
                    } else if (value.compare("100kbps") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "100kbps bitrate detected");
                        bitrate = 100000;
                        bitrateFound = true;
                    } else if (value.compare("125kbps") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "125kbps bitrate detected");
                        bitrate = 125000;
                        bitrateFound = true;
                    } else if (value.compare("250kbps") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "250kbps bitrate detected");
                        bitrate = 250000;
                        bitrateFound = true;
                    } else if (value.compare("500kbps") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "500kbps bitrate detected");
                        bitrate = 500000;
                        bitrateFound = true;
                    } else if (value.compare("800kbps") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "800kbps bitrate detected");
                        bitrate = 800000;
                        bitrateFound = true;
                    } else if (value.compare("1Mbps") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "1Mbps bitrate detected");
                        bitrate = 1000000;
                        bitrateFound = true;
                    } else {
                        errorString = "Unknown bitrate value: ";
                        errorString += value;
                        return false;  // critical error, we need to cancel the request processing right here
                    }
                } else if (key.compare("attackType") == 0) {
                    if (value.compare("PASSTHROUGH") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "PASSTHROUGH attack type detected");
                        attackType = AttackType::PASSTHROUGH;
                        attackTypeFound = true;
                    } else if (value.compare("REPLACE_DATA_WITH_CONSTANT_VALUES") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "REPLACE_DATA_WITH_CONSTANT_VALUES attack type detected");
                        attackType = AttackType::REPLACE_DATA_WITH_CONSTANT_VALUES;
                        attackTypeFound = true;
                    } else if (value.compare("REPLACE_DATA_WITH_RANDOM_VALUES") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "REPLACE_DATA_WITH_RANDOM_VALUES attack type detected");
                        attackType = AttackType::REPLACE_DATA_WITH_RANDOM_VALUES;
                        attackTypeFound = true;
                    } else if (value.compare("ADD_DELTA_VALUE_TO_THE_DATA") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "ADD_DELTA_VALUE_TO_THE_DATA attack type detected");
                        attackType = AttackType::ADD_DELTA_VALUE_TO_THE_DATA;
                        attackTypeFound = true;
                    } else if (value.compare("SUBTRACT_DELTA_VALUE_FROM_THE_DATA") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "SUBTRACT_DELTA_VALUE_FROM_THE_DATA attack type detected");
                        attackType = AttackType::SUBTRACT_DELTA_VALUE_FROM_THE_DATA;
                        attackTypeFound = true;
                    } else if (value.compare("INCREASE_DATA_UNTIL_MAX_VALUE") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "INCREASE_DATA_UNTIL_MAX_VALUE attack type detected");
                        attackType = AttackType::INCREASE_DATA_UNTIL_MAX_VALUE;
                        attackTypeFound = true;
                    } else if (value.compare("DECREASE_DATA_UNTIL_MIN_VALUE") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "DECREASE_DATA_UNTIL_MIN_VALUE attack type detected");
                        attackType = AttackType::DECREASE_DATA_UNTIL_MIN_VALUE;
                        attackTypeFound = true;
                    } else if (value.compare("REPLACE_DATA_WITH_INCREASING_COUNTER") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "REPLACE_DATA_WITH_INCREASING_COUNTER attack type detected");
                        attackType = AttackType::REPLACE_DATA_WITH_INCREASING_COUNTER;
                        attackTypeFound = true;
                    } else if (value.compare("REPLACE_DATA_WITH_DECREASING_COUNTER") == 0) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "REPLACE_DATA_WITH_DECREASING_COUNTER attack type detected");
                        attackType = AttackType::REPLACE_DATA_WITH_DECREASING_COUNTER;
                        attackTypeFound = true;
                    } else {
                        errorString = "Unknown attack type: ";
                        errorString += value;
                        return false;  // critical error, we need to cancel the request processing right here
                    }
                } else if (key.compare("id") == 0) {
                    if (regex_match(value, std::regex("^[0-9a-fA-F]{1,8}$"))) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "ID detected: %s", value.c_str());
                        idToBeAttacked = strtoul(value.c_str(), NULL, 16);
                        idFound = true;
                    } else { //These are not critical error, because maybe we don't even need this parameter. The error is being handled later in the code
                        parseErrorString += "Bad id value: ";
                        parseErrorString += value;
                        parseErrorString += ", expected a 1-8 char long hex value. ";
                    }
                } else if (key.compare("offset") == 0) {
                    if (regex_match(value, std::regex("^[0-7]$"))) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "Offset detected: %s", value.c_str());
                        offset = strtoul(value.c_str(), NULL, 10);
                        attackOffsetFound = true;
                    } else {
                        parseErrorString += "Bad offset value: ";
                        parseErrorString += value;
                        parseErrorString += ", expected a number between 0-7. ";
                    }
                } else if (key.compare("attackLength") == 0) {
                    if (regex_match(value, std::regex("^[1-8]$"))) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "Attack length detected: %s", value.c_str());
                        attackLength = strtoul(value.c_str(), NULL, 10);
                        attackLengthFound = true;
                    } else {
                        parseErrorString += "Bad attackLength value: ";
                        parseErrorString += value;
                        parseErrorString += ", expected a number between 1-8. ";
                    }
                } else if (key.compare("byteValue") == 0) {
                    if (regex_match(value, std::regex("^[0-9a-fA-F]{1,2}$"))) {
                        ESP_LOGD(WEBSERVER_LOG_TAG, "Byte value detected: %s", value.c_str());
                        byteValue = strtoul(value.c_str(), NULL, 16);
                        byteValueFound = true;
                    } else {
                        parseErrorString += "Bad byteValue value: ";
                        parseErrorString += value;
                        parseErrorString += ", expected 1-2 char long hex value. ";
                    }
                } else {
                    errorString += "Unknown parameter type set: ";
                    errorString += key;
                    return false; //we don't allow unknown parameters, maybe they made a typo. Teminate request processing
                }
            }

            // Check if the correct parameters are set
            if(bitrateFound){
                if (attackTypeFound) {
                    switch (attackType) {
                        case AttackType::PASSTHROUGH:
                            // No parameter needed
                            break;

                        case AttackType::REPLACE_DATA_WITH_CONSTANT_VALUES:
                        case AttackType::ADD_DELTA_VALUE_TO_THE_DATA:
                        case AttackType::SUBTRACT_DELTA_VALUE_FROM_THE_DATA:
                            if (!byteValueFound || !idFound || !attackOffsetFound || !attackLengthFound) {
                                if (parseErrorString.empty()){
                                    errorString = "At least one of the required parameters (id, offset, attackLength, byteValue) is missing. ";
                                } else {
                                    errorString = "At least one of the required parameters (id, offset, attackLength, byteValue) is missing or had parsing error. Parsing error message: " + parseErrorString;
                                }
                                return false;
                            }
                            break;

                        case AttackType::REPLACE_DATA_WITH_RANDOM_VALUES:
                        case AttackType::INCREASE_DATA_UNTIL_MAX_VALUE:
                        case AttackType::DECREASE_DATA_UNTIL_MIN_VALUE:
                        case AttackType::REPLACE_DATA_WITH_INCREASING_COUNTER:
                        case AttackType::REPLACE_DATA_WITH_DECREASING_COUNTER:
                            if (!idFound || !attackOffsetFound || !attackLengthFound) {
                                if (parseErrorString.empty()) {
                                    errorString = "At least one of the required parameters (id, offset, attackLength) is missing. ";
                                } else {
                                    errorString = "At least one of the required parameters (id, offset, attackLength) is missing or had parsing error. Parsing error message: " + parseErrorString;
                                }
                                return false;
                            }
                            break;

                        default:  // Internal error, should never happen unless you change the code
                            errorString = "Internal error occured: unexpected AttackTypeEnum";
                            return false;
                    }

                    // Validate attack length and offset sum
                    if (attackType != AttackType::PASSTHROUGH && attackOffsetFound && attackLengthFound) {
                        if ((offset + attackLength) > 8) {
                            errorString = "Sum of offset and attack length cannot be greater than 8";
                            return false;
                        }
                    }

                    // At this point, input is parsed into parameters, values have been checked, and
                    // every parameter required by the attack is set.
                    successString = "CAN Gateway configured with the following parameters:";
                    std::stringstream hexStringStream;
                    hexStringStream << std::hex;
        
                    successString += " Bitrate: ";
                    successString += std::to_string(bitrate);

                    successString += ", AttackType: ";
                    successString += std::to_string(static_cast<std::underlying_type<AttackType>::type>(attackType));

                    if (attackType != AttackType::PASSTHROUGH){
                        hexStringStream << idToBeAttacked;
                        successString += ", ID: 0x";
                        successString += hexStringStream.str();

                        successString += ", Offset: ";
                        successString += std::to_string(offset);

                        successString += ", AttackLength: ";
                        successString += std::to_string(attackLength);

                        if (attackType == AttackType::REPLACE_DATA_WITH_CONSTANT_VALUES || attackType == AttackType::ADD_DELTA_VALUE_TO_THE_DATA || attackType == AttackType::SUBTRACT_DELTA_VALUE_FROM_THE_DATA) {
                            hexStringStream.str("");
                            hexStringStream << (short)byteValue;
                            successString += ", ByteValue: 0x";
                            successString += hexStringStream.str();
                        }
                    }
                } else {
                    errorString = "No attackType was set! Format example: bitrate=250kbps&attackType=REPLACE_DATA_WITH_CONSTANT_VALUES&id=0a1B2c3D&offset=1&attackLength=2&byteValue=a3";
                    return false;
                }
            } else {
                errorString = "No bitrate was set! Format example: bitrate=250kbps&attackType=REPLACE_DATA_WITH_CONSTANT_VALUES&id=0a1B2c3D&offset=1&attackLength=2&byteValue=a3";
                return false;
            }

        } else {  // Invalid data
            errorString =
                "Invalid data! Format example: bitrate=250kbps&attackType=REPLACE_DATA_WITH_CONSTANT_VALUES&id=0a1B2c3D&offset=1&attackLength=2&byteValue=a3";
            return false;
        }

    } else {  // No data set in request
        errorString = "No data set in request Format example: bitrate=250kbps&attackType=REPLACE_DATA_WITH_CONSTANT_VALUES&id=0a1B2c3D&offset=1&attackLength=2&byteValue=a3";
        return false;
    }

    attackConfig -> setNewAttackConfig(bitrate, attackType, idToBeAttacked, offset, attackLength, byteValue, byteValue);

    return true;
}

esp_err_t WebServerHandler::getRequestHandler(httpd_req_t* req) {
    ESP_LOGI(WEBSERVER_LOG_TAG, "Serving GET request for: %s", req->uri);

    std::string uri = std::string(req->uri);
    if (uri.compare("/") == 0 || uri.compare("/index.html") == 0) {
        httpd_resp_send(req, (const char*)index_html_start, (const char*)index_html_end - (const char*)index_html_start); //we can serve files using the .rodata pointers defined at the top
    } else if (uri.compare("/css/main.css") == 0) {
        httpd_resp_set_type(req, "text/css");
        httpd_resp_send(req, (const char*)main_css_start, (const char*)main_css_end - (const char*)main_css_start);
    } else {
        return httpd_resp_send_404(req);
    }

    return ESP_OK;
}

esp_err_t WebServerHandler::postRequestHandler(httpd_req_t* req) {
    char requestContent[MAX_CONTENT_LENGTH + 1]; //+1 for the terminating 0
    memset(requestContent, 0x00, sizeof(requestContent));

    ESP_LOGI(WEBSERVER_LOG_TAG, "Serving POST request for: %s", req->uri);

    size_t requestContentLength = (uint32_t)req->content_len;

    if (requestContentLength > (uint32_t)sizeof(requestContent)) { //handle too long requests
        std::string resp = "Request too long! Received " + std::to_string(requestContentLength) + " bytes, max size is: " + std::to_string(MAX_CONTENT_LENGTH) + "bytes";
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, resp.c_str(), strlen(resp.c_str()));
        ESP_LOGI(WEBSERVER_LOG_TAG, "%s", resp.c_str());
        return ESP_OK;
    }

    ESP_LOGD(WEBSERVER_LOG_TAG, "Request content length: %d", requestContentLength);

    int receivedBytes = httpd_req_recv(req, requestContent, requestContentLength);
    ESP_LOGD(WEBSERVER_LOG_TAG, "Received bytes: %d", receivedBytes);
    
    if (receivedBytes <= 0) {
        if (receivedBytes == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
            return ESP_FAIL;  // In case of error, returning ESP_FAIL will ensure that the underlying socket is closed
        } else if (receivedBytes == HTTPD_SOCK_ERR_INVALID || receivedBytes == HTTPD_SOCK_ERR_FAIL) {
            std::string resp = "Error during receiving";
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, resp.c_str(), strlen(resp.c_str()));
            ESP_LOGE(WEBSERVER_LOG_TAG, "%s", resp.c_str());
            return ESP_OK;
        } else if (receivedBytes == 0) {
            std::string resp = "Request contains no data";
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_send(req, resp.c_str(), strlen(resp.c_str()));
            ESP_LOGW(WEBSERVER_LOG_TAG, "%s", resp.c_str());
            return ESP_OK;
        }
    }

    ESP_LOGD(WEBSERVER_LOG_TAG, "Request content: %s", requestContent);

    //These strings are passed for the parses function as reference, so both of them can be edited
    std::string successString = "Config successful! ";
    std::string errorString = "";
    if(WebServerHandler::parseRequestContentIntoAttackConfig(requestContent, successString, errorString)){
        ESP_LOGI(WEBSERVER_LOG_TAG, "%s", successString.c_str());
        httpd_resp_sendstr(req, successString.c_str());
        WebServerHandler::configReceived = true;
    } else {
        ESP_LOGW(WEBSERVER_LOG_TAG, "Config parse error: %s", errorString.c_str());

        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, errorString.c_str());
    }
    return ESP_OK;
}

void WebServerHandler::startWebServer() {
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &WebServerHandler::getRootURIDescriptor);  // register every URI descriptor so we actually serve the requests
        httpd_register_uri_handler(server, &WebServerHandler::getIndexURIDescriptor); 
        httpd_register_uri_handler(server, &WebServerHandler::getMainCssURIDescriptor);

        httpd_register_uri_handler(server, &WebServerHandler::postURIDescriptor);
    }
}

void WebServerHandler::stopWebServer() {
    if (this->server) {
        httpd_stop(server);
    }
}

bool WebServerHandler::checkIfConfigReceived(){
    return WebServerHandler::configReceived;
}

WebServerHandler::WebServerHandler(PowerManager* powerManager, AttackConfig* attackConfig) {
    this->config = HTTPD_DEFAULT_CONFIG();
    this->config.max_uri_handlers = WEBSERVER_NUMBER_OF_MAX_URL_HANDLERS;
    this->server = NULL;
    this->powerManager = powerManager;

    WebServerHandler::configReceived = false;
    WebServerHandler::attackConfig = attackConfig;
}