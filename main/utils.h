#ifndef MAIN_UTILS_H_
#define MAIN_UTILS_H_

#include "esp_http_server.h"

#define MACSTREND "%02X:%02X:%02X"
#define MAC2STREND(mac) (mac)[3], (mac)[4], (mac)[5]

//MESH-ID NAO PODE SEPARADOR ':', POIS FORMULARIO HTTP CODIFICA DIFERENTE AO FAZER SUBMIT
#define MESHSTR "%02X-%02X-%02X-%02X-%02X-%02X"
#define MESH2STR(mesh_id) (mesh_id)[0],(mesh_id)[1],(mesh_id)[2],(mesh_id)[3],(mesh_id)[4],(mesh_id)[5]


void print_chip_info(void);
void mac_str_to_bytes(const char *mac_str, uint8_t *mac_bytes);
void mac_bytes_to_string(uint8_t mac[6], char *mac_str);
bool compare_mac(uint8_t mac1[6], uint8_t mac2[6]);
void httpd_resp_send_str_chunk(httpd_req_t *req, char *response);
void httpd_resp_send_format_str_chunk(httpd_req_t *req, char *buffer, size_t buffer_size, const char *format, ...);
void format_mac(char *str, const uint8_t *mac);
void send_ws_frame_str(httpd_req_t *req, char *payload);
esp_err_t receive_ws_frame_str(httpd_req_t *req, char *payload);

#endif /* MAIN_UTILS_H_ */
