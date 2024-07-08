#ifndef MAIN_MESSAGES_H_
#define MAIN_MESSAGES_H_

#include <inttypes.h>
#include "cJSON.h"

typedef enum {
    MSG_NODE_CONNECTED = 1,
} MESSAGE_ID_t;

void tx_msg_node_connected(char* buf, int buf_size, uint8_t node_sta_addr[6], uint8_t parent_sta_addr[6], int layer);
bool rx_msg_node_connected(cJSON *root, uint8_t node_sta_addr[6], uint8_t parent_sta_addr[6], int *layer);
bool rx_msg_fw_update_request(char* payload, char* fw_url);

#endif /* MAIN_MESSAGES_H_ */
