#ifndef HTML_BUILDER_H_
#define HTML_BUILDER_H_

#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_mesh.h"


void send_root_html(
    httpd_req_t *req, 
    uint8_t sta_addr[6], //mac address deste dispositivo 
    char* router_ssid, 
    char* router_password, 
    uint8_t mesh_id[6],
    char* mesh_password,
    mesh_addr_t* routing_table,
    int routing_table_size
);

void send_set_wifi_html(httpd_req_t *req, bool valid_setting);

#endif