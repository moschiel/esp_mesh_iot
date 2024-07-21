# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

# Define the HTML files
# @minify_config
# Use this if enabled USE_HTML_FROM_EMBED_TXTFILES and USE_MIN_HTML
set(HTML_FILES "${CMAKE_SOURCE_DIR}/html/min_index.html" "${CMAKE_SOURCE_DIR}/html/min_mesh_graph.html")
# Use this if enabled USE_HTML_FROM_EMBED_TXTFILES and USE_FULL_HTML
#set(HTML_FILES "${CMAKE_SOURCE_DIR}/html/index.html" "${CMAKE_SOURCE_DIR}/html/mesh_graph.html")

idf_component_register(
    SRCS main.c app_config.c web_server.c web_socket.c utils.c mesh_network.c  config_ip_addr.c messages.c mesh_tree.c ota.c
    INCLUDE_DIRS        "." # optional, add here public include directories
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES            spiffs esp_wifi esp_http_server esp_http_client esp_https_ota driver nvs_flash json app_update
    PRIV_REQUIRES       # optional, list the private requirements
    EMBED_TXTFILES  ${HTML_FILES} #use this if enabled USE_HTML_FROM_EMBED_TXTFILES
)

# Use this if enabled USE_HTML_FROM_SPIFFS
# add HTML content to 'spiffs/storage' partition
#spiffs_create_partition_image(storage ../html FLASH_IN_PROJECT)
