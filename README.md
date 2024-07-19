
# ESP32 Mesh Network IoT Project

![image](https://github.com/user-attachments/assets/7563f7af-70f5-4488-895f-fd678e61d1da)


This project uses ESP-IDF with ESP32-DevKitV1 to create a mesh network of multiple ESP32 devices. The goal is to provide a connectivity solution for indoor environments with the ability to perform OTA (Over-The-Air) updates and, in the future, to control sensors and actuators.

One of the main challenges and attractions of this project was developing the OTA functionality in the mesh network using only the official ESP-IDF from Espressif. Unlike higher-level frameworks like painlessMesh (Arduino) or ESP-MDF (Espressif), this project is focused on developers who prefer or need to use ESP-IDF only.

## Main Features

1. **Initial Configuration**:
   - When the ESP32 does not find saved WiFi credentials in NVS memory, it starts in access point (AP) mode.
   - The AP SSID is dynamically created in the format "ESP32_Config_XXYYZZ", where `XXYYZZ` are the last three bytes of the STA interface MAC address.
   - The default password is `esp32config`.
   - The user can connect to the ESP WiFi and using a browser access the default IP address of the ESP in AP mode (`192.168.4.1`) to configure the initial credentials.

2. **Connection to Mesh Network**:
   - After the initial configuration, the ESP32 restarts and attempts to connect to the mesh network using the provided credentials.
   - In Mesh mode, the ESP32 that is the main node of the mesh network hosts a web server.
   - The root node can be accessed via a browser if connected to the same WiFi router as the ESP, using the static IP configured.

3. **Web Server**:
   - Allows the user to configure the WiFi router and mesh network credentials through an HTML page (index.html).
   - Enables the firmware update of nodes in the mesh network by providing the firmware URL.
   - Provides a tree view of the mesh network (mesh_graph.html), showing the parent-child relationships between nodes.

4. **OTA Update**:
   - The user can send the firmware URL to update all nodes in the mesh network.
   - A socket is opened between the browser and the root ESP, allowing the user to monitor the update progress.

5. **Manual Switching Between AP (Access Point) Mode and Mesh Mode**:
   - It is possible to manually switch between AP and Mesh modes through a pin described in the code.
   - An LED indicates the connection status and operation mode.

## Interpreting the LED

The LED indicates the current state of the ESP32. The LED used is the default LED of the DevKit1 module:

- **LED flashing very fast (10 times per second)**: This indicates that the Esp32 is not connected and is trying to connect to the mesh network or is trying to init AP mode. If this state keep for too long, something is wrong.
- **LED flashing rapidly (5 times per second)**: This indicates that the button (pin D5) has been connected to GND and held long enough to change the esp32 mode. (AP mode / MESH mode)
![WhatsApp Video 2024-07-19 at 18 00 28](https://github.com/user-attachments/assets/7dd2ba66-d8a0-4c68-8436-7d66273f339d)

- **LED flashing slowly (1 time per second)**: The ESP32 is on AP (Access Point) mode.
![WhatsApp-Video-2024-07-19-at-17 54 49](https://github.com/user-attachments/assets/7ca663ab-8ca8-4c21-9800-ceb6c9a6d88c)

- **LED on continuously**: The ESP32 is connected to the mesh network and operating normally.
- **LED off**: The ESP32 is not connected or is in an error state.

## HTML Storage

The project supports three methods for storing and serving HTML files that are placed on 'html' folder:

1. **USE_HTML_FROM_EMBED_TXTFILES**
   - Uses embedded text files to store the HTML.
   - Files are read and embedded into the binary during compilation.

2. **USE_HTML_FROM_MACROS**
   - Uses macros to store the HTML.
   - HTML is converted into macros defined in `html_macros.h` and included directly in the source code.

3. **USE_HTML_FROM_SPIFFS**
   - Uses SPIFFS (SPI Flash File System) to store the HTML.
   - HTML is stored as files in the SPIFFS, allowing easy updates and modifications.
   - Atention: the HTML files are not updated Over-the-Air in this mode, this functionality still has to be implemented.

To define which method to use, the user must modify the `web_server.h` file and set only one of the defines to 1. For example:

```c
#define USE_HTML_FROM_EMBED_TXTFILES 1
#define USE_HTML_FROM_MACROS 0
#define USE_HTML_FROM_SPIFFS 0
```

## Web Server Routes

1. **`/`**
   - **Method:** GET
   - **Handler:** `root_get_handler`
   - **Description:** Returns the main page (index.html) of the web server, it is the initial configuration page where the user can configure the WiFi router and mesh network credentials.

2. **`/get_configs`**
   - **Method:** GET
   - **Handler:** `root_get_configs_handler`
   - **Description:** Returns the current device configurations, including WiFi and mesh network credentials.

3. **`/mesh_list_data`**
   - **Method:** GET
   - **Handler:** `mesh_list_data_handler`
   - **Description:** Returns JSON data presenting a list of all devices connected to the mesh network, including the MAC addresses of the nodes, layer and firmware version.

4. **`/set_wifi`**
   - **Method:** POST
   - **Handler:** `set_wifi_post_handler`
   - **Description:** Receives and configures the WiFi router and mesh network credentials. This route is used to send the data from the initial configuration form.

5. **`/mesh_tree_view`**
   - **Method:** GET
   - **Handler:** `mesh_tree_view_handler`
   - **Description:** Returns a tree view of the mesh network topology. This page allows the user to visualize the parent-child relationships between nodes in the mesh network.

6. **`/mesh_tree_data`**
   - **Method:** GET
   - **Handler:** `mesh_tree_data_handler`
   - **Description:** Provides the data needed to generate the mesh network tree view. This route returns JSON data with the network structure.

7. **`/ws_update`**
   - **Method:** GET
   - **Handler:** `ws_update_fw_handler`
   - **Description:** Opens a socket to update the firmware of the devices in the mesh network. This route is used to initiate the OTA update process and monitor the progress.

## Project Structure

- **main/**: Contains the main source code files.
  - `app_config.c`, `app_config.h`: Application configurations.
  - `config_ip_addr.c`, `config_ip_addr.h`: IP address configuration.
  - `html_macros.h`: HTML macros.
  - `main.c`: Main file.
  - `mesh_network.c`, `mesh_network.h`: Mesh network management.
  - `mesh_tree.c`, `mesh_tree.h`: Mesh network tree view.
  - `messages.c`, `messages.h`: Message management.
  - `ota.c`, `ota.h`: OTA implementation.
  - `utils.c`, `utils.h`: Utility functions.
  - `web_server.c`, `web_server.h`: Web server implementation.
  - `web_socket.c`, `web_socket.h`: Web socket implementation.

- **html/**: Contains the HTML files.

## Configuration Tutorial

1. **Prerequisites**:
   - At least two ESP32-DevKitV1 (One is going to be the root node, add others to grow the mesh network)
   - Configured ESP-IDF development environment
   - Python installed (for the OTA update server)

2. **Initial Configuration**:
   - Connect the ESP32 and run the project. The ESP32 will start in AP mode if no saved credentials are found.
   - Connect to the ESP32 WiFi using the SSID in the format "ESP32_Config_XXYYZZ" and the password `esp32config`.
   - Access the default IP address (`192.168.4.1`).
   - Configure the WiFi router and mesh network credentials on the configuration page.

3. **Connecting to the Mesh Network**:
   - After configuring the credentials, the ESP32 will restart and attempt to connect to the mesh network.
   - Access the static IP address configured for the ESP32 to view the configuration and update options.

4. **OTA Update**:
   - Run a local web server on your computer to host the .bin firmware file.
     ```bash
     python -m http.server 8000
     ```
   - Access the OTA update page through the ESP32 web server and provide the firmware URL.

5. **Viewing the Mesh Network**:
   - Access `mesh_graph.html` to view the mesh network topology in tree format.


## Building and Flashing the Project

1. Clone the repository and navigate to the project directory:
   ```bash
   git clone <repository>
   cd esp_mesh_iot
   ```

2. Build and flash the project to the ESP32:
   ```bash
   idf.py build
   idf.py flash
   ```

3. Monitor the ESP32 output:
   ```bash
   idf.py monitor
   ```

---

This is a basic guide to get started with the project. Make sure to adjust the configurations as needed and explore the code to understand all the functionalities in detail.
