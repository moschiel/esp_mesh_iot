
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "web_socket.h"

#define MAX_CLIENTS 3

static const char *TAG = "WEBSOCKET";
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
    char* msg;
};
extern httpd_handle_t webserver_handle;


//Reply Websocket Request
void reply_req_ws_frame_str(httpd_req_t *req, char *payload)
{
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = (uint8_t*)payload;
    ws_pkt.len = strlen(payload);
    httpd_ws_send_frame(req, &ws_pkt);
}


static void tx_ws_msg_async(void *arg)
{
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)resp_arg->msg;
    ws_pkt.len = strlen(resp_arg->msg);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg->msg);
    free(resp_arg);
}

void add_ws_msg_to_tx_queue(char *msg)
{
    // Send async message to all connected clients 
    if (webserver_handle == NULL) { // httpd might not have been created by now
        return;
    }
    size_t clients = MAX_CLIENTS;
    int client_fds[MAX_CLIENTS];
    if (httpd_get_client_list(webserver_handle, &clients, client_fds) == ESP_OK) {
        for (size_t i=0; i < clients; ++i) {
            int sock = client_fds[i];
            if (httpd_ws_get_fd_info(webserver_handle, sock) == HTTPD_WS_CLIENT_WEBSOCKET) {
                ESP_LOGI(TAG, "Active client (fd=%d) -> sending async message", sock);
                struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
                resp_arg->msg = malloc(strlen(msg) + 1);
                resp_arg->hd = webserver_handle;
                resp_arg->fd = sock;
                strcpy(resp_arg->msg, msg);
                if (httpd_queue_work(resp_arg->hd, tx_ws_msg_async, resp_arg) != ESP_OK) {
                    ESP_LOGE(TAG, "httpd_queue_work failed!");
                    break;
                }
            }
        }
    } else {
        ESP_LOGE(TAG, "httpd_get_client_list failed!");
    }
}