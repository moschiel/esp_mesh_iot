#include "esp_http_server.h"

void reply_req_ws_frame_str(httpd_req_t *req, char *payload);
void add_ws_msg_to_tx_queue(char *msg);