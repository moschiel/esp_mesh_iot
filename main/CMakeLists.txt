# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS main.c app_config.c web_server.c utils.c mesh_network.c  config_ip_addr.c messages.c mesh_tree.c ota.c
    INCLUDE_DIRS        "." # optional, add here public include directories
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES            spiffs esp_wifi esp_http_server esp_http_client esp_https_ota driver nvs_flash json # optional, list the public requirements (component names)
    PRIV_REQUIRES       # optional, list the private requirements
)

spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)
