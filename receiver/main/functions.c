// functions.c
// Place all general purpose functions in this module. Please remember to add this module
// to the CMakeLists.txt file or it won't get compiled and linked! You'll also get "undefined
// reference" errors in the compile!
//

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include <lwip/netdb.h>

// These are defined via the menuconfig. Use idf.py menuconfig
#define EXAMPLE_ESP_MAXIMUM_RETRY CONFIG_ESP_MAXIMUM_RETRY

#define PORT 8266

// These macros are defined in setup.c. Redefine here for event_handler.
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

extern const char *TAG;
extern EventGroupHandle_t s_wifi_event_group;

// This is our event handler function. We are going to use this function
// to catch various events (both WIFI_EVENT and IP_EVENT) and respond
// accordingly. If a WIFI_EVENT_STA_START event is posted to the default event
// loop then that means the WiFi driver has started and we should go ahead and
// start the WiFi connection (esp_wifi_connect). If we receive a WIFI_EVENT_STA_DISCONNECTED
// event then that means we disconnected and we will try to reconnect (up to the max attempts).
// And finally, if we receive an IP_EVENT_STA_GOT_IP then we know we have a successful 
// client connection; so we'll update our event group and set the bit for WIFI_CONNECTED_BIT!
// See setup.c for use of event_handler.
void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{

  int s_retry_num = 0;

  if (event_base == WIFI_EVENT)
      ESP_LOGI(TAG, "WiFi Event: %d", event_id);
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
      esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
      if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
          esp_wifi_connect();
          s_retry_num++;
          ESP_LOGI(TAG, "Retrying the connection to the AP");
      } else {
          xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
      }
      ESP_LOGI(TAG,"Connect to the AP failed!");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
      ESP_LOGI(TAG, "Got IP: %s", ip4addr_ntoa(&event->ip_info.ip));
      s_retry_num = 0;
      xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

// This is our UDP server function. It is executed as a FreeRTOS task in an infinite loop. Do not
// exit or return from this function or you will get an error on the console and the SoC will
// continually reboot! See receiver_main.c for task creation code.
void udp_server_task ()
{
  char rx_buffer[128];
  char addr_str[128];
  int ip_protocol = 0;
  struct sockaddr_in dest_addr;
  int sock = -1;
  int result, len; 
  struct sockaddr_in source_addr;
  socklen_t socklen;
 
  // Just loop forever ... or until something goes wrong ... remember ... we don't want
  // to exit or return from this function. We'll call this loop "Setup and Create"
  while (1) {

    // Setup the socket ... let's listen on any available IP and port 8266. IPv4 only.
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    ip_protocol = IPPROTO_IP;

    // Now create the socket
    sock = socket(AF_INET, SOCK_DGRAM, ip_protocol);

    if (sock < 0) {
      ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
      break;
    }

    ESP_LOGI(TAG, "Socket created");

    // And bind the socket to the IP and port
    result = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    if (result < 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
    }

    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    // Obtain and process the incoming datagrams. We'll call this loop "Process the Data".
    while (1) {

      ESP_LOGI(TAG, "Waiting for data ...");
      socklen = sizeof(source_addr);
      len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

      // Error occurred during receiving
      if (len < 0) {
        ESP_LOGE(TAG, "Receive data failed: errno %d", errno);
        break;
       }

      // You got data!
      else {
        // Get the sender's ip address as string
        inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
        // Null-terminate whatever we received and treat like a string.
        rx_buffer[len] = 0;
        ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
        ESP_LOGI(TAG, "%s", rx_buffer);

      }

    } // End of "Process the Data"

    if (sock != -1) {
      ESP_LOGE(TAG, "Shutting down socket and restarting...");
      shutdown(sock, 0);
      close(sock);
    }

  } // End of "Setup and Create"

  vTaskDelete(NULL);

}
