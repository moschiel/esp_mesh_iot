#ifndef MAIN_MESSAGES_H_
#define MAIN_MESSAGES_H_

#include <inttypes.h>
#include "cJSON.h"

typedef enum {
    JSON_MSG_NODE_STATUS = 1,
    JSON_MSG_OTA_OFFSET_ERR = 2,
    JSON_MSG_SET_WIFI_CONFIGS = 3,
} JSON_MSG_ID_t;

typedef enum {
    BIN_MSG_FW_PACKET = 0x0102,
} BIN_MSG_ID_t;

typedef struct {
    uint16_t id;        // Identificador da mensagem
    char version[32];
    uint32_t offset;
    uint32_t data_size;
    uint32_t total_size;
    uint8_t data[1024];
} __attribute__((packed)) firmware_packet_t;

bool mount_msg_node_status(char* buf, int buf_size, uint8_t node_sta_addr[6], uint8_t parent_sta_addr[6], int layer, char* fw_ver);
bool mount_msg_get_wifi_config(char* buf, int buf_size);
bool mount_msg_ota_status(char* buf, int buf_size, char* msg, bool done, bool isError, uint8_t percent);
bool send_ws_msg_ota_status(char* msg, bool done, bool isError, uint8_t percent);
bool process_msg_node_status(cJSON *root);
bool process_msg_set_wifi_config(cJSON *json);
bool process_msg_fw_update_request(char* payload);
void process_msg_firmware_packet(firmware_packet_t *packet);
bool process_msg_ota_offset_err(cJSON *root);
esp_err_t mesh_broadcast_json_msg(char* jsonString);
esp_err_t mesh_broadcast_bin_msg(uint8_t* buf, uint16_t size);
esp_err_t mesh_root_json_msg(char* jsonString);

#endif /* MAIN_MESSAGES_H_ */
