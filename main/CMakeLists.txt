# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS main.c app_config.c app_modes.c web_server.c utils.c        # list the source files of this component
    INCLUDE_DIRS        # optional, add here public include directories
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES            esp_wifi esp_http_server driver nvs_flash # optional, list the public requirements (component names)
    PRIV_REQUIRES       # optional, list the private requirements
)
