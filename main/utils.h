#ifndef MAIN_UTILS_H_
#define MAIN_UTILS_H_

#include "esp_http_server.h"

#define MACSTREND "%02X:%02X:%02X"
#define MAC2STREND(mac) (mac)[3], (mac)[4], (mac)[5]


void print_chip_info(void);
void mac_str_to_bytes(const char *mac_str, uint8_t *mac_bytes);
void mac_bytes_to_string(uint8_t mac[6], char *mac_str);
bool compare_mac(uint8_t mac1[6], uint8_t mac2[6]);
void httpd_str_resp_send_chunk(httpd_req_t *req, char *response);
void httpd_str_format_resp_send_chunk(httpd_req_t *req, char *buffer, size_t buffer_size, const char *format, ...);
void format_mac(char *str, const uint8_t *mac);

#endif /* MAIN_UTILS_H_ */
