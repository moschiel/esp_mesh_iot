#include <string.h>

#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_mesh.h"

#include "utils.h"
#include "html_builder.h"

//#define USE_TREE_EXAMPLE
#ifdef USE_TREE_EXAMPLE
char* html_table_example = 
"<tr><td>DE:2E:94</td><td>WiFi Router</td><td>1</td></tr>"
"<tr><td>58:67:48</td><td>DE:2E:94</td> <td>2</td></tr>"
"<tr><td>DD:80:9C</td><td>DE:2E:94</td> <td>2</td></tr>"
"<tr><td>DC:58:7A</td><td>58:67:48</td> <td>3</td></tr>"
"<tr><td>EC:52:78</td><td>58:67:48</td> <td>3</td></tr>"
"<tr><td>AC:53:1A</td><td>58:67:48</td> <td>3</td></tr>"
"<tr><td>3E:A7:E8</td><td>DD:80:9C</td> <td>3</td></tr>"
"<tr><td>67:58:39</td><td>DD:80:9C</td> <td>3</td></tr>"
"<tr><td>12:CD:E8</td><td>EC:52:78</td> <td>4</td></tr>"
"<tr><td>B2:84:8D</td><td>67:58:39</td> <td>4</td></tr>"
"<tr><td>E4:83:2D</td><td>EC:52:78</td> <td>4</td></tr>";

char* html_json_example = "{\"name\":\"WiFi Router\",\"layer\":0,\"children\":[{\"layer\":1,\"name\":\"DE:2E:94\",\"children\":[{\"name\":\"58:67:48\",\"layer\":2,\"children\":[{\"name\":\"DC:58:7A\",\"layer\":3},{\"name\":\"EC:52:78\",\"layer\":3,\"children\":[{\"name\":\"12:CD:E8\",\"layer\":4},{\"name\":\"E4:83:2D\",\"layer\":4}]},{\"name\":\"AC:53:1A\",\"layer\":3}]},{\"name\":\"DD:80:9C\",\"layer\":2,\"children\":[{\"name\":\"3E:A7:E8\",\"layer\":3},{\"name\":\"67:58:39\",\"layer\":3,\"children\":[{\"name\":\"B2:84:8D\",\"layer\":4}]}]}]}]}";

#endif

//When you are finished sending all your chunks, you must call this macro
#define HTTP_RESP_CLOSE_CHUNKS(req) httpd_resp_send_chunk(req, NULL, 0)

void send_root_html(
    httpd_req_t *req, 
    uint8_t sta_addr[6], //station mac address deste dispositivo 
    char* router_ssid, 
    char* router_password, 
    uint8_t mesh_id[6],
    char* mesh_password,
    bool mesh_parent_connected,
    MeshNode* mesh_tree_table,
    int mesh_tree_count)
{
    char aux_buf[500];

    httpd_resp_send_str_chunk(req, 
    "<!DOCTYPE html>\n"
       "<html>\n"
	       "<head>\n"
		       "<meta name='viewport' content='width=device-width, initial-scale=1'>\n"
		       "<style>\n"
                    "body { font-family: Arial, sans-serif; margin: auto; padding: 20px; justify-content: center; align-items: center; background-color: #f0f0f0; display: flex; flex-direction: column;}\n"
                    ".container { width: 100%; max-width: 400px; background-color: #fff; padding: 20px; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.1); border-radius: 8px; text-align: left; }\n"
                    "input[type='text'], input[type='password'] { width: 80%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 4px; font-size: 16px; }\n"
                    "button,input[type='submit'] { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }\n"
                    "button,input[type='submit']:hover { background-color: #45a049; }\n"
                    "table { border-collapse: collapse; width: 100%; }\n"
                    "table, th, td { border:1px solid black; }\n"
                "</style>\n"
                "<script>\n"
                    "function formatMeshID(e) {\n"
                        "var r = e.target.value.replace(/[^a-fA-F0-9]/g, '');\n"
                        "e.target.value = r.match(/.{1,2}/g).join('-');\n"
                    "}\n"
                    "function togglePassword(id) {\n"
                        "var x = document.getElementById(id);\n"
                        "if (x.type === 'password') {\n"
                            "x.type = 'text';\n"
                        "} else {\n"
                            "x.type = 'password';\n"
                        "}\n"
                    "}\n"
                "</script>\n"
	       "</head>\n"
	       "<body>\n"
                "<br/>\n"
                "<div class='container'>\n"
                    "<h2>Device Info</h2>\n");
    httpd_resp_send_format_str_chunk(req, aux_buf, sizeof(aux_buf),
                   "<p>MAC address: "MACSTR"</p>\n", MAC2STR(sta_addr));
    httpd_resp_send_str_chunk(req,         
		       "</div>\n"
               "<br/>\n"
		       "<div class='container'>\n"
			       "<form action='/set_wifi' method='post'>\n"
                        "<h2>Configure WiFi Router</h2>\n"
                        "<label for='router_ssid'>Router SSID:</label><br>\n");
    httpd_resp_send_format_str_chunk(req, aux_buf, sizeof(aux_buf),
                        "<input type='text' id='router_ssid' name='router_ssid' value='%s' required><br>\n", router_ssid);
    httpd_resp_send_str_chunk(req,
                        "<label for='router_password'>Router Password:</label><br>\n");
    httpd_resp_send_format_str_chunk(req, aux_buf, sizeof(aux_buf),                        
                        "<input type='password' id='router_password' name='router_password' value='%s' required><br/>\n", router_password);
    httpd_resp_send_str_chunk(req,
                        "<input type='checkbox' onclick='togglePassword(\"router_password\")'> Show Password<br/>\n"
                        "<br/>\n"
                        "<h2>Configure Mesh Network</h2>\n"
                        "<label for='mesh_id'>Mesh ID:</label><br>\n");
    httpd_resp_send_format_str_chunk(req, aux_buf, sizeof(aux_buf),   //MESH-ID NAO PODE SEPARADOR ':', POIS FORMULARIO CODIFICA DIFERENTE AO FAZER SUBMIT
                        "<input type='text' id='mesh_id' name='mesh_id' value='%02X-%02X-%02X-%02X-%02X-%02X' required oninput='formatMeshID(event)' maxlength='17'><br>\n",
                        mesh_id[0],mesh_id[1],mesh_id[2],mesh_id[3],mesh_id[4],mesh_id[5]);
    httpd_resp_send_str_chunk(req,
                        "<label for='mesh_password'>Mesh Password:</label><br>\n");
    httpd_resp_send_format_str_chunk(req, aux_buf, sizeof(aux_buf),
                        "<input type='password' id='mesh_password' name='mesh_password' value='%s' required><br/>\n", mesh_password);
    httpd_resp_send_str_chunk(req,
                        "<input type='checkbox' onclick='togglePassword(\"mesh_password\")'> Show Password<br/>\n"
                        "<br/>\n"
                        "<input type='submit' value='Update Config'>\n"
			       "</form>\n"
		       "</div>\n");

    if(mesh_parent_connected) {
        httpd_resp_send_str_chunk(req, 
                "<br/>\n"
                "<div class='container'>\n"
                    "<h2>Nodes in Mesh Network</h2>\n"
                    "<button onclick=\"openTree()\">Open Tree View</button><br /><br />\n"
                    "<table>\n"
                        "<tr><th>Node</th><th>Parent</th><th>Layer</th></tr>\n");
        #ifdef USE_TREE_EXAMPLE
        httpd_resp_send_str_chunk(req, html_table_example);
        #else
        httpd_resp_send_format_str_chunk(req, aux_buf, sizeof(aux_buf),
                        "<tr><td>"MACSTREND"</td><td>WiFi Router</td> <td>1</td></tr>\n", MAC2STREND(sta_addr));
        if(mesh_tree_table != NULL && mesh_tree_count > 0) {
            for (int i = 0; i < mesh_tree_count; i++) {
                httpd_resp_send_format_str_chunk(req, aux_buf, sizeof(aux_buf),
                        "<tr><td>"MACSTREND"</td><td>"MACSTREND"</td> <td>%i</td></tr>\n",
                        MAC2STREND(mesh_tree_table[i].node_sta_addr), MAC2STREND(mesh_tree_table[i].parent_sta_addr), mesh_tree_table[i].layer);
            }
        }
        #endif
        
        httpd_resp_send_str_chunk(req, 
                    "</table>\n"
                "</div>\n"
                "<script>\n"
                    "function openTree() {\n"
                        "window.open('mesh_tree', '_blank');\n"
                    "}\n"
                "</script>\n"
                "<br/>\n");
    }

    httpd_resp_send_str_chunk(req, 
            "</body>\n"
        "</html>\n");

    HTTP_RESP_CLOSE_CHUNKS(req);
}

void send_set_wifi_html(httpd_req_t *req, bool valid_setting) {
    if(valid_setting) {
        httpd_resp_send_str_chunk(req, "<!DOCTYPE html>\n"
           "<html>\n"
               "<head>\n"
                   "<meta name='viewport' content='width=device-width, initial-scale=1'>\n"
                   "<style>\n"
                        "body { font-family: Arial, sans-serif; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; background-color: #f0f0f0; }\n"
                        ".container { text-align: center; background-color: #fff; padding: 20px; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.1); border-radius: 8px; }\n"
                   "</style>\n"
               "</head>\n"
               "<body>\n"
                   "<div class='container'>\n"
                       "<h2>WiFi settings updated</h2>\n"
                       "<p>Restarting...</p>\n"
                   "</div>\n"
               "</body>\n"
           "</html>\n");
    } else {
        httpd_resp_send_str_chunk(req, "Invalid input");
    }

    HTTP_RESP_CLOSE_CHUNKS(req);
}


void send_mesh_tree_html(
    httpd_req_t *req,
    char *mesh_json_str) 
{
    httpd_resp_send_str_chunk(req,
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
            "<meta charset=\"UTF-8\">\n"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
            "<title>Mesh Network Tree</title>\n"
            "<style>\n"
                "html { height: 100%; }\n"
                "body { font-family: Arial, sans-serif; margin: auto; padding: 20px; background-color: #f4f4f4; height: 100%; }\n"
                "h1 { color: #333; text-align: center; }\n"
                ".container { width: 100%; max-width: 1600px; margin: 0 auto; height: 100%; display: flex; flex-direction: column; }\n"
                ".svg-container { width: 100%; height: 100%; border: 1px solid #ddd; border-radius: 4px; background-color: #fff; position: relative; }\n"
                "svg { width: 100%; height: 100%; }\n"
                ".node { cursor: pointer; }\n"
                ".node rect { stroke: steelblue; stroke-width: 3px; rx: 10; ry: 10; }\n"
                ".node text { font: 12px sans-serif; font-weight: bold; }\n"
                ".link { fill: none; stroke: #666; }\n"
                "button { margin: 20px auto; padding: 10px 20px; font-size: 16px; cursor: pointer; display: block; }\n"
                "@media (max-width: 600px) {\n"
                    ".node rect { stroke-width: 6px; width: 220px; height: 60px; x: -110px; y: -40px; }\n"
                    ".node text { font: 40px sans-serif; font-weight: bold; }\n"
                    ".link { stroke-width: 4px; }\n"
                "}\n"
                "@media (min-width: 601px) {\n"
                    ".node rect { stroke-width: 3px; width: 120px; height: 30px; x: -60px; y: -20px; }\n"
                    ".node text { font: 18px sans-serif; font-weight: bold; }\n"
                    ".link { stroke-width: 2px; }\n"
                "}\n"
            "</style>\n"
        "</head>\n"
        "<body>\n"
            "<div class=\"container\">\n"
                "<h1>Mesh Network Tree</h1>\n"
                "<button id=\"toggle-button\">Collapse All</button>\n"
                "<div class=\"svg-container\">\n"
                    "<svg viewBox=\"0 0 960 800\" preserveAspectRatio=\"xMidYMid meet\">\n"
                        "<g class=\"zoomable\">\n"
                            //conteudo do gráfico será adicionado aqui
                        "</g>\n"
                    "</svg>\n"
                "</div>\n"
            "</div>\n"
            "<script src=\"https://d3js.org/d3.v6.min.js\"></script>\n"
            "<script>\n"
                "const data = ");
    #ifdef USE_TREE_EXAMPLE
    httpd_resp_send_str_chunk(req, html_json_example);
    #else
    httpd_resp_send_str_chunk(req, mesh_json_str);
    #endif
    httpd_resp_send_str_chunk(req,
                ";\n"
                "const svg = d3.select(\"svg\");\n"
                "const width = +svg.attr(\"viewBox\").split(\" \")[2];\n"
                "const height = +svg.attr(\"viewBox\").split(\" \")[3];\n"
                "const zoomable = svg.select(\".zoomable\");\n"
                "const zoom = d3.zoom()\n"
                    ".scaleExtent([0.1, 10])\n"
                    ".on(\"zoom\", (event) => {\n"
                        "zoomable.attr(\"transform\", event.transform);\n"
                    "});\n"
                "svg.call(zoom);\n"
                "const g = zoomable.append(\"g\").attr(\"transform\", \"translate(40,40)\");\n"
                "const tree = d3.tree().size([height, width - 160]);\n"
                "const root = d3.hierarchy(data, d => d.children);\n"
                "root.x0 = height / 2;\n"
                "root.y0 = 0;\n"
                "let i = 0;\n"
                "let allCollapsed = false;\n"
                "function collapse(d) {\n"
                    "if (d.children) {\n"
                        "d._children = d.children;\n"
                        "d._children.forEach(collapse);\n"
                        "d.children = null;\n"
                    "}\n"
                "}\n"
                "function expand(d) {\n"
                    "if (d._children) {\n"
                        "d.children = d._children;\n"
                        "d.children.forEach(expand);\n"
                        "d._children = null;\n"
                    "}\n"
                "}\n"
                "function collapseAll(d) {\n"
                    "collapse(d);\n"
                    "if (d.children) {\n"
                        "d.children.forEach(collapseAll);\n"
                    "}\n"
                    "if (d._children) {\n"
                        "d._children.forEach(collapseAll);\n"
                    "}\n"
                "}\n"
                "function expandAll(d) {\n"
                    "expand(d);\n"
                    "if (d.children) {\n"
                        "d.children.forEach(expandAll);\n"
                    "}\n"
                "}\n"
                "function getColor(layer) {\n"
                    "switch (layer) {\n"
                        "case 0: return '#ffcccc';\n"
                        "case 1: return '#ffebcc';\n"
                        "case 2: return '#ffffcc';\n"
                        "case 3: return '#ccffcc';\n"
                        "case 4: return '#cceeff';\n"
                        "case 5: return '#ccccff';\n"
                        "default: return '#fff';\n"
                    "}\n"
                "}\n"
                "function getClass(layer) {\n"
                    "switch (layer) {\n"
                        "default: return 'node';\n"
                    "}\n"
                "}\n"
                "if(root.children){\n"
                    "root.children.forEach(allCollapsed ? collapse : expand);\n"
                "}\n"
                "update(root);\n"
                "document.getElementById('toggle-button').addEventListener('click', () => {\n"
                    "if (allCollapsed) {\n"
                        "expandAll(root);\n"
                        "update(root);\n"
                        "document.getElementById('toggle-button').textContent = 'Collapse All';\n"
                    "} else {\n"
                        "collapseAll(root);\n"
                        "update(root);\n"
                        "document.getElementById('toggle-button').textContent = 'Expand All';\n"
                    "}\n"
                    "allCollapsed = !allCollapsed;\n"
                "});\n"
                "function update(source) {\n"
                    "const treeData = tree(root);\n"
                    "const nodes = treeData.descendants();\n"
                    "const links = treeData.descendants().slice(1);\n"
                    "nodes.forEach(d => {\n"
                        "d.y = d.depth * 180;\n"
                    "});\n"
                    "const node = g.selectAll('g.node').data(nodes, d => d.id || (d.id = ++i));\n"
                    "const nodeEnter = node.enter().append('g').attr('class', d => getClass(d.data.layer)).attr('transform', d => `translate(${source.x0},${source.y0})`).on('click', (event, d) => {\n"
                        "if (d.children) {\n"
                            "d._children = d.children;\n"
                            "d.children = null;\n"
                        "} else {\n"
                            "d.children = d._children;\n"
                            "d._children = null;\n"
                        "}\n"
                        "update(d);\n"
                    "});\n"
                    "nodeEnter.append('rect')\n"
                        ".attr('fill', d => getColor(d.data.layer))\n"
                        ".attr('class', d => getClass(d.data.layer));\n"
                    "nodeEnter.append('text').style('text-anchor', 'middle').text(d => d.data.name);\n"
                    "const nodeUpdate = nodeEnter.merge(node);\n"
                    "nodeUpdate.transition().duration(750).attr('transform', d => `translate(${d.x},${d.y})`);\n"
                    "nodeUpdate.select('rect')\n"
                        ".attr('fill', d => getColor(d.data.layer))\n"
                        ".attr('class', d => getClass(d.data.layer));\n"
                    "const nodeExit = node.exit().transition().duration(750).attr('transform', d => `translate(${source.x},${source.y})`).remove();\n"
                    "const link = g.selectAll('path.link').data(links, d => d.id);\n"
                    "const linkEnter = link.enter().insert('path', 'g').attr('class', 'link').attr('d', d => {\n"
                        "const o = {x: source.x0, y: source.y0};\n"
                        "return diagonal(o, o);\n"
                    "});\n"
                    "linkEnter.transition().duration(750).attr('d', d => diagonal(d, d.parent));\n"
                    "link.transition().duration(750).attr('d', d => diagonal(d, d.parent));\n"
                    "link.exit().transition().duration(750).attr('d', d => {\n"
                        "const o = {x: source.x, y: source.y};\n"
                        "return diagonal(o, o);\n"
                    "}).remove();\n"
                    "nodes.forEach(d => {\n"
                        "d.x0 = d.x;\n"
                        "d.y0 = d.y;\n"
                    "});\n"
                    "function diagonal(s, d) {\n"
                        "return `M${s.x},${s.y}\n"
                                "C${s.x},${(s.y + d.y) / 2}\n"
                                 "${d.x},${(s.y + d.y) / 2}\n"
                                 "${d.x},${d.y}`;\n"
                    "}\n"
                "}\n"
            "</script>\n"
        "</body>\n"
        "</html>");

    HTTP_RESP_CLOSE_CHUNKS(req);
}