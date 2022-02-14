#ifndef WEBSERVERHANDLER_H
#define WEBSERVERHANDLER_H

#include <esp_http_server.h>

#include "PowerManager.h"
#include "AttackConfig.h"

#include <string>

/**
 * This class can create and start a webserver, receive the configuration via POST request, parse it, and set it for the MessageAttacker.
 */
class WebServerHandler {
   private:
    PowerManager* powerManager;

    static httpd_uri_t getRootURIDescriptor;
    static httpd_uri_t getIndexURIDescriptor;
    static httpd_uri_t getMainCssURIDescriptor;

    static httpd_uri_t postURIDescriptor;

    httpd_config_t config;
    httpd_handle_t server;

    static bool configReceived;

    static AttackConfig* attackConfig;

   public:
    WebServerHandler(PowerManager* powerManager, AttackConfig* attackConfig);

    static esp_err_t getRequestHandler(httpd_req_t* req);
    static esp_err_t postRequestHandler(httpd_req_t* req);
    static bool parseRequestContentIntoAttackConfig(const char* requestContent,
                                                                      std::string& successString,
                                                                      std::string& errorString);

    void startWebServer();
    void stopWebServer();

    bool checkIfConfigReceived();
};

#endif