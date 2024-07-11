# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

# Define the minified HTML output files
set(HTML_FILES "${CMAKE_SOURCE_DIR}/html/index.html" "${CMAKE_SOURCE_DIR}/html/mesh_graph.html")
set(MINIFIED_HTML_FILES "${CMAKE_SOURCE_DIR}/html/min_index.html" "${CMAKE_SOURCE_DIR}/html/min_mesh_graph.html")

idf_component_register(
    SRCS main.c app_config.c web_server.c utils.c mesh_network.c  config_ip_addr.c messages.c mesh_tree.c ota.c
    INCLUDE_DIRS        "." # optional, add here public include directories
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES            spiffs esp_wifi esp_http_server esp_http_client esp_https_ota driver nvs_flash json # optional, list the public requirements (component names)
    PRIV_REQUIRES       # optional, list the private requirements
    # EMBED_TXTFILES  ${HTML_FILES}
    EMBED_TXTFILES ${MINIFIED_HTML_FILES}
)

# descomentar abaixo se for armazenar HTML da pasta 'html' na particao 'storage'
#spiffs_create_partition_image(storage ../html FLASH_IN_PROJECT)
