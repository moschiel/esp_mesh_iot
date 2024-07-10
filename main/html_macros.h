
#ifndef HTML_MACROS_H_
#define HTML_MACROS_H_

#if USE_HTML_FROM_MACROS

/* START HTML MACROS AREA */

/*
 * Automatically generated code by 'HTML-to-Macro-String-Converter v1.0'
 * containing the HTML code converted to macros.
 * The parameters are:
 *    Minify: false
 *    Output: ./main/html_macros.h
 *    Input[1]: ./html/index.html, Macro: INDEX_HTML
 *    Input[2]: ./html/mesh-graph.html, Macro: TREE_VIEW_HTML
 *
 * Project Repository: https://github.com/moschiel/html-to-macro-string
 */

#define INDEX_HTML \
"<!DOCTYPE html>\n"\
"<html>\n"\
"<head>\n"\
"	<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />\n"\
"	<style>\n"\
"		body {\n"\
"			font-family: Arial, sans-serif;\n"\
"			margin: auto;\n"\
"			padding: 20px;\n"\
"			justify-content: center;\n"\
"			align-items: center;\n"\
"			background-color: #f0f0f0;\n"\
"			display: flex;\n"\
"			flex-direction: column;\n"\
"		}\n"\
"		.container {\n"\
"			width: 100%;\n"\
"			max-width: 400px;\n"\
"			background-color: #fff;\n"\
"			padding: 20px;\n"\
"			box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.1);\n"\
"			border-radius: 8px;\n"\
"			text-align: left;\n"\
"		}\n"\
"		input[type='text'],\n"\
"		input[type='password'] {\n"\
"			width: 80%;\n"\
"			padding: 10px;\n"\
"			margin: 10px 0px;\n"\
"			border: 1px solid #ccc;\n"\
"			border-radius: 4px;\n"\
"			font-size: 16px;\n"\
"		}\n"\
"		button, input[type='submit'] {\n"\
"			background-color: #4CAF50;\n"\
"			color: white;\n"\
"			padding: 10px 20px;\n"\
"			border: none;\n"\
"			border-radius: 4px;\n"\
"			cursor: pointer;\n"\
"			font-size: 16px;\n"\
"		}\n"\
"		button:hover, input[type='submit']:hover {\n"\
"			background-color: #45a049;\n"\
"		}\n"\
"		table { border-collapse: collapse; width: 100%; }\n"\
"		table, th, td { border:1px solid black; }\n"\
"	</style>\n"\
"	<script>\n"\
"		let ws;\n"\
"\n"\
"		function formatMeshID(e) { \n"\
"			var r = e.target.value.replace(/[^a-fA-F0-9]/g, ''); \n"\
"			e.target.value = r.match(/.{1,2}/g).join('-'); \n"\
"		}\n"\
"		function togglePassword(id) { \n"\
"			var x = document.getElementById(id); \n"\
"			if (x.type === 'password') { \n"\
"				x.type = 'text'; \n"\
"			} else { \n"\
"				x.type = 'password'; \n"\
"			} \n"\
"		}\n"\
"		// Função para buscar o JSON de diversas configuracoes\n"\
"		function fetchJsonConfigs() {\n"\
"			fetch('/get_configs') // Alterar a URL conforme necessário\n"\
"				.then(response => response.json())\n"\
"				.then(data => applyConfigs(data))\n"\
"				.catch(error => console.error('Error fetching data:', error));\n"\
"		}\n"\
"		// Função para construir a tabela a partir do JSON\n"\
"		function applyConfigs(data) {\n"\
"			document.getElementById('device_mac_addr').value = data.device_mac_addr;\n"\
"			document.getElementById('router_ssid').value = data.router_ssid;\n"\
"			document.getElementById('router_password').value = data.router_password;\n"\
"			document.getElementById('mesh_id').value = data.mesh_id;\n"\
"			document.getElementById('mesh_password').value = data.mesh_password;\n"\
"			document.getElementById('firmware_url').value = data.fw_url;\n"\
"		}\n"\
"		// Função para buscar o JSON da rede mesh no servidor\n"\
"		function fetchJsonMeshList() {\n"\
"			fetch('/mesh_list_data')\n"\
"				.then(response => response.json())\n"\
"				.then(data => buildMeshTable(data))\n"\
"				.catch(error => console.error('Error fetching data:', error));\n"\
"		}\n"\
"		// Função para construir a tabela a partir do JSON\n"\
"		function buildMeshTable(data) {\n"\
"			let table = document.getElementById('meshTable');\n"\
"			table.innerHTML = '<tr><th>Node</th><th>Parent</th><th>Layer</th></tr>';\n"\
"			if(data.nodes) {\n"\
"				data.nodes.forEach(node => {\n"\
"					let row = table.insertRow();\n"\
"					row.insertCell(0).textContent = node.name;\n"\
"					row.insertCell(1).textContent = node.parent;\n"\
"					row.insertCell(2).textContent = node.layer;\n"\
"				});\n"\
"			}\n"\
"		}\n"\
"		// Função para enviar a URL do firmware e acompanhar a atualização\n"\
"		function sendFirmwareURL() {\n"\
"			if(ws != null && ws.readyState === WebSocket.OPEN) \n"\
"				return;\n"\
"\n"\
"			const url = document.getElementById('firmware_url').value;\n"\
"			const ota_status = document.getElementById('ota_status');\n"\
"			ws = new WebSocket('ws://' + window.location.host + '/ws_update');\n"\
"\n"\
"			ws.onopen = function() {\n"\
"				ota_status.textContent = 'Socket Handshake';\n"\
"				ws.send(JSON.stringify({ fw_url: url }));\n"\
"			};\n"\
"			ws.onmessage = function(event) {\n"\
"				const data = JSON.parse(event.data);\n"\
"				if (data.msg) {\n"\
"					ota_status.textContent = data.msg;\n"\
"				}\n"\
"				if (data.done) {\n"\
"					ws.close();\n"\
"				}\n"\
"			};\n"\
"			ws.onerror = function(error) {\n"\
"				console.error('WebSocket Error: ', error);\n"\
"				ota_status.textContent = 'Update Failed!';\n"\
"			};\n"\
"			ws.onclose = function() {\n"\
"				console.log('WebSocket Closed');\n"\
"			};\n"\
"		}\n"\
"		// função para enviar os dados do formulario\n"\
"		function submitConfigForm(event) {\n"\
"			event.preventDefault();\n"\
"			const formData = new FormData(event.target);\n"\
"			const params = new URLSearchParams();\n"\
"			for (const pair of formData.entries()) {\n"\
"				params.append(pair[0], pair[1]);\n"\
"			}\n"\
"			const statusConfig = document.getElementById('status_config');\n"\
"			statusConfig.textContent = 'Command sent';\n"\
"			fetch('/set_wifi', {\n"\
"				method: 'POST',\n"\
"				body: params.toString(),\n"\
"				headers: {\n"\
"					'Content-Type': 'application/x-www-form-urlencoded'\n"\
"				}\n"\
"			})\n"\
"			.then(response => {\n"\
"				if (response.ok) {\n"\
"					statusConfig.textContent = 'WiFi settings updated, Restarting...';\n"\
"				} else {\n"\
"					statusConfig.textContent = 'Error setting configs';\n"\
"				}\n"\
"			})\n"\
"			.catch(error => {\n"\
"				statusConfig.textContent = 'Error sending configs';\n"\
"				console.error('Error:', error);\n"\
"			});\n"\
"		}\n"\
"		// Chama a função de busca de JSON após o carregamento da página\n"\
"		document.addEventListener('DOMContentLoaded', fetchJsonConfigs);\n"\
"		document.addEventListener('DOMContentLoaded', fetchJsonMeshList);\n"\
"	</script>\n"\
"</head>\n"\
"<body>\n"\
"	<br />\n"\
"	<p>v1.5</p>\n"\
"	<div class=\"container\">\n"\
"		<h2>Device Info</h2>\n"\
"		<p >Root MAC address: <span id=\"device_mac_addr\">08:d1:f9:dd:80:9c</span></p>\n"\
"	</div>\n"\
"	<br />\n"\
"	<div class=\"container\">\n"\
"		<form onsubmit=\"submitConfigForm(event)\">\n"\
"			<h2>Configure WiFi Router</h2>\n"\
"			<label for=\"router_ssid\">Router SSID:</label><br />\n"\
"			<input type=\"text\" id=\"router_ssid\" name=\"router_ssid\" required /><br />\n"\
"			<label for=\"router_password\">Router Password:</label><br />\n"\
"			<input type=\"password\" id=\"router_password\" name=\"router_password\" required /><br />\n"\
"			<input type=\"checkbox\" onclick='togglePassword(\"router_password\")' /> Show Password<br />\n"\
"			<br /><br />\n"\
"			<h2>Configure Mesh Network</h2>\n"\
"			<label for=\"mesh_id\">Mesh ID:</label><br />\n"\
"			<input type=\"text\" id=\"mesh_id\" name=\"mesh_id\" required oninput=\"formatMeshID(event)\" maxlength=\"17\" /><br />\n"\
"			<label for=\"mesh_password\">Mesh Password:</label><br />\n"\
"			<input type=\"password\" id=\"mesh_password\" name=\"mesh_password\" required /><br />\n"\
"			<input type=\"checkbox\" onclick='togglePassword(\"mesh_password\")' /> Show Password<br />\n"\
"			<br />\n"\
"			<input type=\"submit\" value=\"Update Config\" />\n"\
"			<p id=\"status_config\"></p>\n"\
"		</form>\n"\
"	</div>\n"\
"	<br />\n"\
"	<div class=\"container\">\n"\
"		<h2>Update Firmware</h2>\n"\
"		<label for=\"firmware_url\">Firmware URL:</label><br />\n"\
"		<input type=\"text\" id=\"firmware_url\" name=\"firmware_url\" required /><br />\n"\
"		<button onclick=\"sendFirmwareURL()\">Update Firmware</button>\n"\
"		<p id=\"ota_status\">Update Status: Not Started</p>\n"\
"	</div>\n"\
"	<br />\n"\
"	<div class=\"container\">\n"\
"		<h3>Nodes in Mesh Network</h3>\n"\
"		<button onclick=\"openTree()\">Open Tree View</button>\n"\
"		<button onclick=\"fetchJsonMeshList()\">Refresh</button>\n"\
"		<br /><br />\n"\
"		<table id=\"meshTable\">\n"\
"			<!-- Tabela será preenchida dinamicamente -->\n"\
"		</table>\n"\
"	</div>\n"\
"	<script>\n"\
"		function openTree() {\n"\
"			window.open('mesh_tree_view', '_blank');\n"\
"		}\n"\
"	</script>\n"\
"</body>\n"\
"</html>\n"\
""

#define TREE_VIEW_HTML \
"<!DOCTYPE html>\n"\
"<html lang=\"en\">\n"\
"<head>\n"\
"	<meta charset=\"UTF-8\">\n"\
"	<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"\
"	<title>Mesh Network Tree</title>\n"\
"	<style>\n"\
"		html {\n"\
"			height: 100%;\n"\
"		}\n"\
"		body {\n"\
"			font-family: Arial, sans-serif;\n"\
"			margin: auto;\n"\
"			padding: 10px;\n"\
"			background-color: #f4f4f4;\n"\
"			height: 100%;\n"\
"		}\n"\
"		h1 {\n"\
"			color: #333;\n"\
"			text-align: center;\n"\
"		}\n"\
"		button {\n"\
"			background-color: #4CAF50;\n"\
"			color: white;\n"\
"			padding: 10px 20px;\n"\
"			border: none;\n"\
"			border-radius: 4px;\n"\
"			cursor: pointer;\n"\
"			font-size: 16px;\n"\
"		}\n"\
"		button:hover {\n"\
"			background-color: #45a049;\n"\
"		}\n"\
"		.container {\n"\
"			width: 100%;\n"\
"			max-width: 1600px;\n"\
"			margin: 0 auto;\n"\
"			height: 100%;\n"\
"			display: flex;\n"\
"			flex-direction: column;\n"\
"		}\n"\
"		.svg-container {\n"\
"			width: 100%;\n"\
"			height: 100%;\n"\
"			border: 1px solid #ddd;\n"\
"			border-radius: 4px;\n"\
"			background-color: #fff;\n"\
"			position: relative;\n"\
"		}\n"\
"		svg {\n"\
"			width: 100%;\n"\
"			height: 100%;\n"\
"		}\n"\
"		.node {\n"\
"			cursor: pointer;\n"\
"		}\n"\
"		.node rect {\n"\
"			stroke: steelblue;\n"\
"			stroke-width: 3px;\n"\
"			rx: 10;\n"\
"			ry: 10;\n"\
"		}\n"\
"		.node text {\n"\
"			font: 12px sans-serif;\n"\
"			font-weight: bold;\n"\
"		}\n"\
"		.link {\n"\
"			fill: none;\n"\
"			stroke: #666;\n"\
"		}\n"\
"		@media (max-width: 600px) {\n"\
"			.node rect {\n"\
"				stroke-width: 6px;\n"\
"				width: 220px;\n"\
"				height: 60px;\n"\
"				x: -110px;\n"\
"				y: -40px;\n"\
"			}\n"\
"			.node text {\n"\
"				font: 40px sans-serif;\n"\
"				font-weight: bold;\n"\
"			}\n"\
"			.link {\n"\
"				stroke-width: 4px;\n"\
"			}\n"\
"		}\n"\
"		@media (min-width: 601px) {\n"\
"			.node rect {\n"\
"				stroke-width: 3px;\n"\
"				width: 120px;\n"\
"				height: 30px;\n"\
"				x: -60px;\n"\
"				y: -20px;\n"\
"			}\n"\
"			.node text {\n"\
"				font: 18px sans-serif;\n"\
"				font-weight: bold;\n"\
"			}\n"\
"			.link {\n"\
"				stroke-width: 2px;\n"\
"			}\n"\
"		}\n"\
"	</style>\n"\
"</head>\n"\
"<body>\n"\
"	<div class=\"container\">\n"\
"		<p>v1.1</p>\n"\
"		<h1>Mesh Network Tree</h1>\n"\
"		<button id=\"load-button\" onclick=\"fetchTreeData()\">Refresh</button>\n"\
"		<div class=\"svg-container\">\n"\
"			<svg viewBox=\"0 0 960 800\" preserveAspectRatio=\"xMidYMid meet\">\n"\
"				<g class=\"zoomable\">\n"\
"					<!-- conteudo do gráfico será adicionado aqui -->\n"\
"				</g>\n"\
"			</svg>\n"\
"		</div>\n"\
"	</div>\n"\
"	<script src=\"https://d3js.org/d3.v6.min.js\"></script>\n"\
"	<script>\n"\
"		const svg = d3.select(\"svg\");\n"\
"		const width = +svg.attr(\"viewBox\").split(\" \")[2];\n"\
"		const height = +svg.attr(\"viewBox\").split(\" \")[3];\n"\
"		const zoomable = svg.select(\".zoomable\");\n"\
"		const zoom = d3.zoom()\n"\
"			.scaleExtent([0.1, 10])\n"\
"			.on(\"zoom\", (event) => {\n"\
"				zoomable.attr(\"transform\", event.transform);\n"\
"			});\n"\
"		svg.call(zoom);\n"\
"		const g = zoomable.append(\"g\").attr(\"transform\", \"translate(40,40)\");\n"\
"		const tree = d3.tree().size([height, width - 160]);\n"\
"		let i = 0;\n"\
"		function collapse(d) {\n"\
"			if (d.children) {\n"\
"				d._children = d.children;\n"\
"				d._children.forEach(collapse);\n"\
"				d.children = null;\n"\
"			}\n"\
"		}\n"\
"		function expand(d) {\n"\
"			if (d._children) {\n"\
"				d.children = d._children;\n"\
"				d.children.forEach(expand);\n"\
"				d._children = null;\n"\
"			}\n"\
"		}\n"\
"		function getColor(layer) {\n"\
"			switch (layer) {\n"\
"				case 0: return '#ffcccc';\n"\
"				case 1: return '#ffebcc';\n"\
"				case 2: return '#ffffcc';\n"\
"				case 3: return '#ccffcc';\n"\
"				case 4: return '#cceeff';\n"\
"				case 5: return '#ccccff';\n"\
"				default: return '#fff';\n"\
"			}\n"\
"		}\n"\
"		function getClass(layer) {\n"\
"			switch (layer) {\n"\
"				default: return 'node';\n"\
"			}\n"\
"		}\n"\
"		function update(source, root) {\n"\
"			const treeData = tree(root);\n"\
"			const nodes = treeData.descendants();\n"\
"			const links = treeData.descendants().slice(1);\n"\
"			nodes.forEach(d => {\n"\
"				d.y = d.depth * 180;\n"\
"			});\n"\
"			const node = g.selectAll('g.node').data(nodes, d => d.id || (d.id = ++i));\n"\
"			const nodeEnter = node.enter().append('g').attr('class', d => getClass(d.data.layer)).attr('transform', d => `translate(${source.x0},${source.y0})`).on('click', (event, d) => {\n"\
"				if (d.children) {\n"\
"					d._children = d.children;\n"\
"					d.children = null;\n"\
"				} else {\n"\
"					d.children = d._children;\n"\
"					d._children = null;\n"\
"				}\n"\
"				update(d, root);\n"\
"			});\n"\
"			nodeEnter.append('rect')\n"\
"				.attr('fill', d => getColor(d.data.layer))\n"\
"				.attr('class', d => getClass(d.data.layer));\n"\
"			nodeEnter.append('text').style('text-anchor', 'middle').text(d => d.data.name);\n"\
"			const nodeUpdate = nodeEnter.merge(node);\n"\
"			nodeUpdate.transition().duration(750).attr('transform', d => `translate(${d.x},${d.y})`);\n"\
"			nodeUpdate.select('rect')\n"\
"				.attr('fill', d => getColor(d.data.layer))\n"\
"				.attr('class', d => getClass(d.data.layer));\n"\
"			const nodeExit = node.exit().transition().duration(750).attr('transform', d => `translate(${source.x},${source.y})`).remove();\n"\
"			const link = g.selectAll('path.link').data(links, d => d.id);\n"\
"			const linkEnter = link.enter().insert('path', 'g').attr('class', 'link').attr('d', d => {\n"\
"				const o = { x: source.x0, y: source.y0 };\n"\
"				return diagonal(o, o);\n"\
"			});\n"\
"			linkEnter.transition().duration(750).attr('d', d => diagonal(d, d.parent));\n"\
"			link.transition().duration(750).attr('d', d => diagonal(d, d.parent));\n"\
"			link.exit().transition().duration(750).attr('d', d => {\n"\
"				const o = { x: source.x, y: source.y };\n"\
"				return diagonal(o, o);\n"\
"			}).remove();\n"\
"			nodes.forEach(d => {\n"\
"				d.x0 = d.x;\n"\
"				d.y0 = d.y;\n"\
"			});\n"\
"			function diagonal(s, d) {\n"\
"				return `M${s.x},${s.y}\n"\
"					C${s.x},${(s.y + d.y) / 2}\n"\
"					${d.x},${(s.y + d.y) / 2}\n"\
"					${d.x},${d.y}`;\n"\
"			}\n"\
"		}\n"\
"		function fetchTreeData() {\n"\
"			d3.json('/mesh_tree_data').then(data => {\n"\
"				g.selectAll('*').remove();\n"\
"				const root = d3.hierarchy(data, d => d.children);\n"\
"				root.x0 = height / 2;\n"\
"				root.y0 = 0;\n"\
"				update(root, root);\n"\
"			}).catch(error => {\n"\
"				console.error('Error fetching mesh data:', error);\n"\
"			});\n"\
"		}\n"\
"		// Chama a função de busca de JSON após o carregamento da página\n"\
"		document.addEventListener('DOMContentLoaded', fetchTreeData);\n"\
"	</script>\n"\
"</body>\n"\
"</html>\n"\
""

/* END HTML MACROS AREA */

#endif // USE_HTML_FROM_MACROS
#endif // HTML_MACROS_H_

