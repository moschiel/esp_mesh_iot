#ifndef WEB_SERVER_H
#define WEB_SERVER_H


#define USE_HTML_FROM_EMBED_TXTFILES    1
#define USE_HTML_FROM_MACROS            0 
#define USE_HTML_FROM_SPIFFS            0 //SPIFFS teria que fazer mais um OTA para a particao SPIFFS.

void start_webserver(bool init_wifi_ap);
void stop_webserver(void);
bool is_webserver_active(void);

#endif // WEB_SERVER_H
