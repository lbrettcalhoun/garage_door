#include "lwip/sockets.h"
#include <lwip/netdb.h>

#define PORT 8266

extern const char *TAG;

// This is our UDP server function. It is executed as a FreeRTOS task in an infinite loop. Do not
// exit or return from this function or you will get an error on the console and the SoC will
// continually reboot!
static void udp_server_task ()
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
