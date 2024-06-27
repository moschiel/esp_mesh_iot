#include <string.h>

#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_mesh.h"

#include "utils.h"

void send_root_html(
    httpd_req_t *req, 
    uint8_t sta_addr[6], //mac address deste dispositivo 
    char* router_ssid, 
    char* router_password, 
    uint8_t mesh_id[6],
    char* mesh_password,
    mesh_addr_t* routing_table,
    int routing_table_size)
{
    char aux_buf[500];

    httpd_str_resp_send_chunk(req, 
    "<!DOCTYPE html>"
       "<html>"
	       "<head>"
		       "<meta name='viewport' content='width=device-width, initial-scale=1'>"
		       "<style>"
			       "body { font-family: Arial, sans-serif; margin: auto; padding: 0; width: 400px; max-width: 90%; justify-content: center; align-items: center; height: 100vh; background-color: #f0f0f0; }"
			       ".container { text-align: center; background-color: #fff; padding: 20px; width: auto; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.1); border-radius: 8px; }"
			       "input[type='text'], input[type='password'] { width: 80%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 4px; font-size: 16px; }"
			       "input[type='submit'] { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }"
			       "input[type='submit']:hover { background-color: #45a049; }"
		       "</style>"
                "<script>"
                    "function formatMeshID(e) {"
                    "  var r = e.target.value.replace(/[^a-fA-F0-9]/g, '');"
                    "  e.target.value = r.match(/.{1,2}/g).join('-');"
                    "}"
                    "function togglePassword(id) {"
                    "  var x = document.getElementById(id);"
                    "  if (x.type === 'password') {"
                        "x.type = 'text';"
                    "  } else {"
                        "x.type = 'password';"
                    "  }"
                    "}"
                "</script>"
	       "</head>"
	       "<body>"
                "<br/>"
                "<div class='container'>"
                    "<h2>Device Info</h2>");
    httpd_str_format_resp_send_chunk(req, aux_buf, sizeof(aux_buf),
                   "<p>MAC address: "MACSTR"</p>", MAC2STR(sta_addr));
    httpd_str_resp_send_chunk(req,         
		       "</div>"
               "<br/>"
		       "<div class='container'>"
			       "<form action='/set_wifi' method='post'>"
                        "<h2>Configure WiFi Router</h2>"
                        "<label for='router_ssid'>Router SSID:</label><br>");
    httpd_str_format_resp_send_chunk(req, aux_buf, sizeof(aux_buf),
                        "<input type='text' id='router_ssid' name='router_ssid' value='%s' required><br>", router_ssid);
    httpd_str_resp_send_chunk(req,
                        "<label for='router_password'>Router Password:</label><br>");
    httpd_str_format_resp_send_chunk(req, aux_buf, sizeof(aux_buf),                        
                        "<input type='password' id='router_password' name='router_password' value='%s' required><br/>", router_password);
    httpd_str_resp_send_chunk(req,
                        "<input type='checkbox' onclick='togglePassword(\"router_password\")'> Show Password<br/>"
                        "<br/>"
                        "<h2>Configure Mesh Network</h2>"
                        "<label for='mesh_id'>Mesh ID:</label><br>");
    httpd_str_format_resp_send_chunk(req, aux_buf, sizeof(aux_buf),   //MESH-ID NAO PODE SEPARADOR ':', POIS FORMULARIO CODIFICA DIFERENTE AO FAZER SUBMIT
                        "<input type='text' id='mesh_id' name='mesh_id' value='%02X-%02X-%02X-%02X-%02X-%02X' required oninput='formatMeshID(event)' maxlength='17'><br>",
                        mesh_id[0],mesh_id[1],mesh_id[2],mesh_id[3],mesh_id[4],mesh_id[5]);
    httpd_str_resp_send_chunk(req,
                        "<label for='mesh_password'>Mesh Password:</label><br>");
    httpd_str_format_resp_send_chunk(req, aux_buf, sizeof(aux_buf),
                        "<input type='password' id='mesh_password' name='mesh_password' value='%s' required><br/>", mesh_password);
    httpd_str_resp_send_chunk(req,
                        "<input type='checkbox' onclick='togglePassword(\"mesh_password\")'> Show Password<br/>"
                        "<br/>"
                        "<input type='submit' value='Update Config'>"
			       "</form>"
		       "</div>");

    if(routing_table != NULL && routing_table_size > 0) {
        httpd_str_resp_send_chunk(req, 
                "<br/>"
                "<div class='container'>"
                    "<h2>Nodes in Mesh Network</h2>"
                    "<ul>");

        for (int i = 0; i < routing_table_size; i++) {
            httpd_str_format_resp_send_chunk(req, aux_buf, sizeof(aux_buf),
                        "<li>"MACSTR"</li>", MAC2STR(routing_table[i].addr));
        }
        
        httpd_str_resp_send_chunk(req, 
                    "</ul>"
                "</div>"
                "<br/>");
    }

    httpd_str_resp_send_chunk(req, 
            "</body>"
        "</html>");

    //When you are finished sending all your chunks, you must call this function with buf_len as 0.
    httpd_resp_send_chunk(req, NULL, 0);
}

void send_set_wifi_html(httpd_req_t *req, bool valid_setting) {
    if(valid_setting) {
        httpd_resp_send(req, "<!DOCTYPE html>"
           "<html>"
               "<head>"
                   "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                   "<style>"
                        "body { font-family: Arial, sans-serif; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; background-color: #f0f0f0; }"
                        ".container { text-align: center; background-color: #fff; padding: 20px; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.1); border-radius: 8px; }"
                   "</style>"
               "</head>"
               "<body>"
                   "<div class='container'>"
                       "<h2>WiFi settings updated</h2>"
                       "<p>Restarting...</p>"
                   "</div>"
               "</body>"
           "</html>", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send(req, "Invalid input", HTTPD_RESP_USE_STRLEN);
    }
}

#if BLABLA
void send_mesh_tree_html(httpd_req_t *req, mesh_addr_t* routing_table, int table_size) {
    char aux_buf[500];

    // Obter o endereço MAC do nó raiz
    mesh_addr_t root_addr;
    esp_mesh_get_mac(&root_addr);

    // Iniciar o HTML com o nó raiz
    httpd_str_resp_send_chunk(req,
        "<!DOCTYPE html>"
        "<html lang=\"en\">"
        "<head>"
            "<meta charset=\"UTF-8\">"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
            "<title>Mesh Network Graph</title>"
            "<style>"
                ".node { cursor: pointer; }"
                ".node rect { stroke: steelblue; stroke-width: 3px; rx: 10; ry: 10; }"
                ".node text { font: 12px sans-serif; }"
                ".link { fill: none; stroke: #ccc; stroke-width: 2px; }"
                "button { margin: 20px; padding: 10px 20px; font-size: 16px; cursor: pointer; }"
            "</style>"
        "</head>"
        "<body>"
            "<h1>Mesh Network Graph</h1>"
            "<button id=\"toggle-button\">Collapse All</button>"
            "<script src=\"https://d3js.org/d3.v6.min.js\"></script>"
            "<script>"
                "const data = {");
    httpd_str_format_resp_send_chunk(req, aux_buf, sizeof(aux_buf),
                    "\"name\": \"" MACSTR "\","
                    "\"children\": [", MAC2STR(root_addr.addr));

    // Função auxiliar para adicionar nós ao HTML
    void add_node_to_html(const mesh_addr_t *node_addr, int level) {
        httpd_str_format_resp_send_chunk(req, aux_buf, sizeof(aux_buf), 
                            "{"
                                "\"name\": \"" MACSTR "\","
                                "\"layer\": %d,"
                                "\"children\": [", MAC2STR(node_addr->addr), level);

        // Obter nós filhos (nós que têm este nó como pai)
        for (int i = 0; i < table_size; i++) {
            mesh_addr_t parent_addr;
            esp_mesh_get_parent_bssid(&parent_addr);
            if (memcmp(routing_table[i].addr, node_addr->addr, 6) == 0) {
                add_node_to_html(&routing_table[i], level + 1);
                httpd_str_resp_send_chunk(req, 
                                "]");
            }
        }

        httpd_str_resp_send_chunk(req, 
                    "]");
    }

    // Adicionar nós filhos do nó raiz
    for (int i = 0; i < table_size; i++) {
        mesh_addr_t parent_addr;
        esp_mesh_get_parent_bssid(&parent_addr);
        if (memcmp(parent_addr.addr, root_addr.addr, 6) == 0) {
            add_node_to_html(&routing_table[i], 1);
            httpd_str_resp_send_chunk(req, "]");
        }
    }

    // Finalizar o HTML
    httpd_str_resp_send_chunk(req,
                "]"
            "};"
            "const width = 960;"
            "const height = 800;"
            "const svg = d3.select(\"body\").append(\"svg\")"
                ".attr(\"width\", width)"
                ".attr(\"height\", height)"
                ".append(\"g\")"
                ".attr(\"transform\", \"translate(40,40)\");"
            "const tree = d3.tree().size([height - 80, width - 160]);"
            "const root = d3.hierarchy(data, d => d.children);"
            "root.x0 = (height - 80) / 2;"
            "root.y0 = 0;"
            "let i = 0;"
            "let allCollapsed = false;"
            "function collapse(d) {"
                "if (d.children) {"
                    "d._children = d.children;"
                    "d._children.forEach(collapse);"
                    "d.children = null;"
                "}"
            "}"
            "function expand(d) {"
                "if (d._children) {"
                    "d.children = d._children;"
                    "d.children.forEach(expand);"
                    "d._children = null;"
                "}"
            "}"
            "function collapseAll(d) {"
                "collapse(d);"
                "if (d.children) {"
                    "d.children.forEach(collapseAll);"
                "}"
                "if (d._children) {"
                    "d._children.forEach(collapseAll);"
                "}"
            "}"
            "function expandAll(d) {"
                "expand(d);"
                "if (d.children) {"
                    "d.children.forEach(expandAll);"
                "}"
            "}"
            "root.children.forEach(collapse);"
            "update(root);"
            "document.getElementById('toggle-button').addEventListener('click', () => {"
                "if (allCollapsed) {"
                    "expandAll(root);"
                    "update(root);"
                    "document.getElementById('toggle-button').textContent = 'Collapse All';"
                "} else {"
                    "collapseAll(root);"
                    "update(root);"
                    "document.getElementById('toggle-button').textContent = 'Expand All';"
                "}"
                "allCollapsed = !allCollapsed;"
            "});"
            "function update(source) {"
                "const treeData = tree(root);"
                "const nodes = treeData.descendants();"
                "const links = treeData.descendants().slice(1);"
                "nodes.forEach(d => {"
                    "d.y = d.depth * 180;"
                "});"
                "const node = svg.selectAll('g.node')"
                    ".data(nodes, d => d.id || (d.id = ++i));"
                "const nodeEnter = node.enter().append('g')"
                    ".attr('class', 'node')"
                    ".attr('transform', d => `translate(${source.x0},${source.y0})`)"
                    ".on('click', (event, d) => {"
                        "if (d.children) {"
                            "d._children = d.children;"
                            "d.children = null;"
                        "} else {"
                            "d.children = d._children;"
                            "d._children = null;"
                        "}"
                        "update(d);"
                    "});"
                "nodeEnter.append('rect')"
                    ".attr('width', 120)"
                    ".attr('height', 30)"
                    ".attr('x', -60)"
                    ".attr('y', -15)"
                    ".attr('fill', d => {"
                        "switch (d.data.layer) {"
                            "case 0: return '#ffcccc';"
                            "case 1: return '#ffebcc';"
                            "case 2: return '#ffffcc';"
                            "case 3: return '#ccffcc';"
                            "case 4: return '#cceeff';"
                            "case 5: return '#ccccff';"
                            "default: return '#fff';"
                        "}"
                    "});"
                "nodeEnter.append('text')"
                    ".attr('dy', 3)"
                    ".attr('x', 0)"
                    ".style('text-anchor', 'middle')"
                    ".text(d => d.data.name);"
                "const nodeUpdate = nodeEnter.merge(node);"
                "nodeUpdate.transition()"
                    ".duration(750)"
                    ".attr('transform', d => `translate(${d.x},${d.y})`);"
                "nodeUpdate.select('rect')"
                    ".attr('width', 120)"
                    ".attr('height', 30)"
                    ".attr('x', -60)"
                    ".attr('y', -15)"
                    ".attr('fill', d => {"
                        "switch (d.data.layer) {"
                            "case 0: return '#ffcccc';"
                            "case 1: return '#ffebcc';"
                            "case 2: return '#ffffcc';"
                            "case 3: return '#ccffcc';"
                            "case 4: return '#cceeff';"
                            "case 5: return '#ccccff';"
                            "default: return '#fff';"
                        "}"
                    "});"
                "const nodeExit = node.exit().transition()"
                    ".duration(750)"
                    ".attr('transform', d => `translate(${source.x},${source.y})`)"
                    ".remove();"
                "const link = svg.selectAll('path.link')"
                    ".data(links, d => d.id);"
                "const linkEnter = link.enter().insert('path', 'g')"
                    ".attr('class', 'link')"
                    ".attr('d', d => {"
                        "const o = {x: source.x0, y: source.y0};"
                        "return diagonal(o, o);"
                    "});"
                "linkEnter.transition()"
                    ".duration(750)"
                    ".attr('d', d => diagonal(d, d.parent));"
                "link.transition()"
                    ".duration(750)"
                    ".attr('d', d => diagonal(d, d.parent));"
                "link.exit().transition()"
                    ".duration(750)"
                    ".attr('d', d => {"
                        "const o = {x: source.x, y: source.y};"
                        "return diagonal(o, o);"
                    "})"
                    ".remove();"
                "nodes.forEach(d => {"
                    "d.x0 = d.x;"
                    "d.y0 = d.y;"
                "});"
                "function diagonal(s, d) {"
                    "return `M${s.x},${s.y}"
                        "C${s.x},${(s.y + d.y) / 2}"
                        "${d.x},${(s.y + d.y) / 2}"
                        "${d.x},${d.y}`;"
                "}"
            "}"
            "</script>"
        "</body>"
        "</html>");

    // Envia o último chunk indicando o fim da resposta
    httpd_resp_send_chunk(req, NULL, 0);
}
#endif