#ifndef WEB_SERVER_H
#define WEB_SERVER_H

//SPIFFS torna mais facil gerenciar o codigo html, 
//mas teria que fazer mais um OTA para a particao SPIFFS.
#define USE_HTML_FROM_SPIFFS 0 
#define USE_HTML_FROM_SOURCE (!USE_HTML_FROM_SPIFFS) 

void start_webserver(bool init_wifi_ap);
void stop_webserver(void);
bool is_webserver_active(void);

#endif // WEB_SERVER_H
