idf_component_register(
	SRCS "wifi.c"
	INCLUDE_DIRS "include"
	REQUIRES esp_wifi esp_system nvs_flash web_server
)

