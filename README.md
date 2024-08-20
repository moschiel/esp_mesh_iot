
# ESP32 Mesh Control

<!--
![image](https://github.com/user-attachments/assets/7563f7af-70f5-4488-895f-fd678e61d1da) 
![Recorder_29062024_121057](https://github.com/user-attachments/assets/66d2d87f-d40f-40d2-8d06-2e24a43f2b76)
-->
<p align="center">
  <img src="https://github.com/user-attachments/assets/7563f7af-70f5-4488-895f-fd678e61d1da" alt="Example Image" height="350"/>
  <img src="https://github.com/user-attachments/assets/66d2d87f-d40f-40d2-8d06-2e24a43f2b76" alt="Example Image" height="400"/>
</p>


This project uses ESP-IDF with ESP32-DevKitV1 to create a mesh network of multiple ESP32 devices. Currently, it only supports local indoor use, so it does not rely on an internet connection. Users need a device (like a smartphone or computer) connected to the same WiFi router as the mesh network to monitor and control it through an HTTP server hosted by the ESP32 itself.

A key challenge of this project was developing OTA (Over-the-Air) functionality using only the official ESP-IDF from Espressif. Unlike higher-level frameworks like painlessMesh (Arduino) or ESP-MDF (Espressif), this project is tailored for developers who prefer or need to use ESP-IDF exclusively.

In the future, there are plans to implement remote control capabilities over the internet.


## Main Features

1. **Initial Configuration**:
   - When the ESP32 does not find saved WiFi credentials in NVS memory, it starts in access point (AP) mode.
   - The AP SSID is dynamically created in the format "ESP32_Config_XXYYZZ", where `XXYYZZ` are the last three bytes of the STA interface MAC address.
   - The default password is `esp32config`.
   - The user can connect to the ESP WiFi and use a browser to access the address `http://espmesh.local/` to configure the initial credentials.
   <!-- The user can connect to the ESP WiFi and use a browser to access the address of the ESP in AP mode (`192.168.4.1`) to configure the initial credentials. -->

2. **Connection to Mesh Network**:
   - After the initial configuration, the ESP32 restarts and attempts to connect to the mesh network using the provided credentials.
   - In Mesh mode, the ESP32 that is the main node of the mesh network hosts a web server.
   - The root node can be accessed with a browser at address `http://espmesh.local/` if connected to the same WiFi router as the ESP.
   <!-- The root node can be accessed with browser if connected to the same WiFi router as the ESP, using the static IP configured. -->

3. **Web Server**:
   - Allows the user to configure the WiFi router and mesh network credentials through an HTML page (index.html).
   - Enables the firmware update of nodes in the mesh network by providing the firmware URL.
   - Provides a tree view of the mesh network (mesh_graph.html), showing the parent-child relationships between nodes.

4. **OTA Update**:
   - The user can send the firmware URL to update all nodes in the mesh network.
   - A socket is opened between the browser and the root ESP, allowing the user to monitor the update progress.

## Interpreting the LED

The LED indicates the current state of the ESP32. The LED used is the default LED of the DevKit1 module:

- **LED blinking very fast (~5 times per second)**: This indicates that the ESP32 is not connected and is trying to connect to the mesh network or is trying to initiate AP mode. If this state persists for too long, try changing the ESP mode manually to Access Point mode to check the configurations, or reset to factory (See section **Troubleshooting Pins** for details)
  
   ![WhatsApp Video 2024-07-19 at 19 05 14](https://github.com/user-attachments/assets/1c8c87c1-aade-4880-aab9-e3226bf6db12)

- **LED blinking rapidly (~2.5 times per second)**: This indicates that one of the troubleshotting pins has been connected to GND and held long enough to accept the command. (See section **Troubleshooting Pins** for details)

   ![WhatsApp Video 2024-07-19 at 18 00 28](https://github.com/user-attachments/assets/7dd2ba66-d8a0-4c68-8436-7d66273f339d)

- **LED blinking slowly (~1 time each 2 seconds)**: The ESP32 is in AP (Access Point) mode. You can set configurations by connecting to the ESP WiFi "ESP32_Config_XXXX" with any device (like a smartphone or computer) and use a browser to access the address `http://espmesh.local/`.

   ![WhatsApp Video 2024-07-19 at 17 54 49](https://github.com/user-attachments/assets/75ca0477-9c05-4266-925d-dc94347368ef)

- **LED on continuously**: The ESP32 is connected to the mesh network and operating normally. You can access it with any device using a browser by typing in the address `http://espmesh.local/`. Your device must be connected to the same WiFi router as the mesh network.
- **LED off**: This should not happen.

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
   - **Atention**: the HTML files are not updated Over-the-Air in this mode, this functionality still has to be implemented.

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
   - At least two ESP32-DevKitV1 (One will be the root node; add more ESP32 devices to grow the mesh network)
   - Configured ESP-IDF development environment
   - Python installed (for running a server that will host a .bin file for the OTA update)

2. **Initial Configuration**:
   - Connect the ESP32 and flash the ESP-IDF project onto the target. The ESP32 will start in AP mode if no saved credentials are found.
   - Connect to the ESP32 WiFi using the SSID in the format "ESP32_Config_XXYYZZ" and the password `esp32config`.
   - Use a browser to access `http://espmesh.local/`.
   <!-- Use a browser to access the default IP address (`192.168.4.1`). -->
   - Enter the credentials used in your WiFi router and set the mesh network credentials on the configuration page.
   - Press `Update Config` to save the configurations. The ESP32 will restart and attempt to connect to the mesh network.
   - If everything goes well, the device will start blinking very fast (searching for the WiFi router), then the LED will turn on continuously. Otherwise, something was configured incorrectly, and you will need to manually switch back to AP mode.
   - Repeat this process for every ESP32 that should connect to the mesh network. It is important to set the same configuration on every device. <!-- (unless you want to set up more than one mesh network). -->

      ![image](https://github.com/user-attachments/assets/89a4b458-e334-4e95-8935-9da08cfabbe6)



3. **Access to Mesh Network**:
   - If the LED is on continuously (not blinking), it is in Mesh mode.
   - Using a device connected to the same WiFi router as the mesh network, open a browser and access `http://espmesh.local/` to view the configuration and update options.
   <!-- - Using a device connected to the same WiFi router as the mesh network, open a browser and access the static IP address configured for the ESP32 to view the configuration and update options. -->
   - If you update the configs here, the configurations will be broadcasted to all devices in the mesh network.
   - If there are multiple devices connected, it is possible to visualize a list of all nodes on the network.
   - Click on `Refresh` button to update the list of connected nodes.

      ![image](https://github.com/user-attachments/assets/215e8b8f-b48b-4258-972d-771cea71f6ef)




3. **Access to Mesh Tree View**:
   - Click on `Open Tree View` or access the route `http://espmesh.local/mesh_tree_view` <!--`http://<your_static_ip_addr>/mesh_tree_view`--> to view the mesh network topology in tree format.
      <!-- ![Recorder_29062024_121057](https://github.com/user-attachments/assets/66d2d87f-d40f-40d2-8d06-2e24a43f2b76) -->
     <p>
        <img src="https://github.com/user-attachments/assets/66d2d87f-d40f-40d2-8d06-2e24a43f2b76" alt="Example Image" height="450"/>
      </p>

## MESH OTA Update Tutorial

1. **OTA Update**:
   - Run a local web server in the project directory on your computer to host the .bin firmware file. For example, you can use the following Python command in the terminal:
     ```bash
     python -m http.server 8000
     ```
   - Access the OTA update page through the ESP32 web server and provide the firmware URL `http://<your_server_ip_addres>:8000/build/app-template.bin`. The root node will update itself, and then broadcast the binary file to all other nodes in the mesh network.
      <p>
        <img src="https://github.com/user-attachments/assets/d0baf17d-1b06-47c1-a24c-e77bd2726fb7" alt="OTA PROGRESS" height="300"/>
      </p>

## Troubleshooting Pins

1. **Manual Switch between AP/MESH modes**:
   - Hold pin D5 to GND until the LED blinks rapidly (~2.5 times per second), then release the pin to change the ESP32 mode (AP mode / MESH mode)
2. **Reseting to Factory**:
   - Hold pin D23 to GND until the LED blinks rapidly (~2.5 times per second), then release the pin to reset NVS memory

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

This is a basic guide to get started with the project. Make sure to adjust the configurations as needed and explore the code to understand all the functionalities in detail.
