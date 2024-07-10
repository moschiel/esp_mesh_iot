# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(app-template)

# Adicione esta seção para executar seu programa de conversão
add_custom_target(
    run_html_to_macro
    COMMAND ${CMAKE_COMMAND} -E echo "Running HTML to Macro String Converter"
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} ${PYTHON} html_to_macro.py true "./main/html_macros.h" "./html/index.html" "INDEX_HTML" "./html/mesh-graph.html" "TREE_VIEW_HTML"
    COMMENT "Executing HTML to Macro String Converter before building"
)

add_dependencies(app run_html_to_macro)