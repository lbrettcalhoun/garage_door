// Receiver
// This is the "receiver" for the garage door project. It acts as a station and 
// connects to the ESP8266 AP. The ESP32 uses ESP-IDF. This IDF is completely 
// different from the ESP8266 non-OS SDK; ESP-IDF actually has an OS: FreeRTOS!
//
// The ESP-IDF uses app_main as the entry point. From app_main, we are going to 
// start the WiFi and then connect to the access point with our function
// wifi_init_sta. We use event groups to monitor the connection. Once we are
// connected, the next step is to create a task (and associated function) for
// our UDP server. The OS will run our task as an infinite loop and we'll receive
// UDP packets in the task function. 

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "setup.h"
#include "functions.h"

// Define a character string for our log messsages
const char *TAG = "Receiver";

void app_main()
{
  esp_err_t ret;
  BaseType_t xTaskReturn;

  // Initialize NVS. We use NVS to store our WiFi configuration. That way the configuration
  // can be preserved across system boots. We also do some error checking to ensure that
  // if we do not first successfully initialize NVS that we try to erase the NVS and then 
  // try again.
  ret = nvs_flash_init();

  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Remind anyone looking at the console output that we are setting up the WiFi for STA mode
  ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

  // Configure and start the WiFi ... includes registering the event handler. See setup.c.
  wifi_init_sta();

  // Create a new FreeRTOS task and add to the task list. The associated function
  // is udp_server_task (see above)) and we'll use 4096 words (NOT BYTES) for the 
  // task stack.  Let's use priority 5 for this task. Remember, tasks are infinite
  // loops; don't try to exit and don't try to return (but it is OK to delete the task).
  // See your above udp_server_task function for more details. Note that udp_server_task is 
  // located in functions.c. See functions.c for details on implementation of the UDP server.
  xTaskReturn = xTaskCreate(udp_server_task,"udp_server",4096,NULL,5,NULL);

  if(xTaskReturn == pdPASS)
  {
    ESP_LOGI(TAG,"UDP server task started\n");
  }
}
