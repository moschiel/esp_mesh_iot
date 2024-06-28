#ifndef HTML_BUILDER_H_
#define HTML_BUILDER_H_

#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_mesh.h"

#include "mesh_tree.h"


void send_root_html(
    httpd_req_t *req, 
    uint8_t sta_addr[6], //mac address deste dispositivo 
    char* router_ssid, 
    char* router_password, 
    uint8_t mesh_id[6],
    char* mesh_password,
    bool mesh_parent_connected,
    MeshNode* mesh_tree,
    int mesh_tree_count
);

void send_set_wifi_html(httpd_req_t *req, bool valid_setting);

void send_mesh_tree_html(
    httpd_req_t *req,
    char *mesh_json_str
); 

#endif