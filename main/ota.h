#ifndef MAIN_OTA_H_
#define MAIN_OTA_H_


#include "esp_http_server.h"

void start_ota(char* url, httpd_req_t *req);
void send_ws_ota_status(httpd_req_t* req, char* msg, bool done);

#endif /* MAIN_OTA_H_ */
