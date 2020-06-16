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
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "functions.c"

// These are defined via the menuconfig. Use idf.py menuconfig
#define EXAMPLE_ESP_WIFI_SSID    CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS    CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT    BIT1

// Define a character string for our log messsages
const char *TAG = "Receiver";

// Number of retry attempts for our WiFi connection
static int s_retry_num = 0;

// Return value for the task
BaseType_t xTaskReturn;

// This is our event handler function. We are going to use this function
// to catch various events (both WIFI_EVENT and IP_EVENT) and respond
// accordingly. If a WIFI_EVENT_STA_START event is posted to the default event
// loop then that means the WiFi driver has started and we should go ahead and
// start the WiFi connection (esp_wifi_connect). If we receive a WIFI_EVENT_STA_DISCONNECTED
// event then that means we disconnected and we will try to reconnect (up to the max attempts).
// And finally, if we receive an IP_EVENT_STA_GOT_IP then we know we have a successful 
// client connection; so we'll update our event group and set the bit for WIFI_CONNECTED_BIT!
static void event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT)
      ESP_LOGI(TAG, "WiFi Event: %d", event_id);
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
      esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
      if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
          esp_wifi_connect();
          s_retry_num++;
          ESP_LOGI(TAG, "retry to connect to the AP");
      } else {
          xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
      }
      ESP_LOGI(TAG,"connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
      ESP_LOGI(TAG, "got ip:%s",
               ip4addr_ntoa(&event->ip_info.ip));
      s_retry_num = 0;
      xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

static void wifi_init_sta()
{
  // Create a new FreeRTOS event group ... s_wifi_event_group is our handle to
  // the group. An event group is nothing more than a set of event bits.
  s_wifi_event_group = xEventGroupCreate();

  // Initialize the TCP/IP stack ... including LwIP ... call this function one time
  // only upon app startup. According to the API Guide, we really should migrate this
  // to NET_IF; but it'll have to do for now.
  tcpip_adapter_init();

  // WiFi events can use the system default event loop ... so ... let's use it!
  // Remember ... for the default event loop the handle is hidden ... you don't
  // need to pass the loop handler to the default event loop functions.
  // Note that ESP_ERROR_CHECK is basically the ESP-IDF "assert" function. It will
  // check to make sure the return value is ESP_OK. If not, it will abort execution.
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Use the macro WIFI_INIT_CONFIG_DEFAULT() to initialize all the various resources
  // for the WiFi driver to their default values (instead of setting them 1 by 1). These
  // resources include things like TX/RX buffers, TX/RX windows, etc.
  // Then call esp_wifi_init() to actually initialize the WiFi driver. Use ESP_ERROR_CHECK()
  // macro to make sure all is well with the WiFi driver (and if not abort).
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // Now let's register our event handler callback functions. Our event handler function is aptly named
  // event_handler (see above static function). We are interested in the following events:
  //  ESP_EVENT_ANY_ID: This is a "catch all" for alerting us to ANY of the various WIFI_EVENT events
  //  IP_EVENT_STA_GOT_IP: The DHCP client on the ESP32 has received a valid IP address 
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

  // In the steps above, we used the macro WIFI_INIT_CONFIG_DEFAULT() to initialize our WiFi driver
  // with various defaults. Now let's set the specific options for our WiFi configuration;
  // such as SSID and PASSWORD. We need to use a wifi_config_t type (instead of a wifi_init_config_t type).  
  wifi_config_t wifi_config = {
      .sta = {
          .ssid = EXAMPLE_ESP_WIFI_SSID,
          .password = EXAMPLE_ESP_WIFI_PASS
      },
  };
  
  // Don't forget to set the WiFi to STATION mode (client)
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  // Next we apply our specific options to the WiFi configuration
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
  // And finally ... it's time to start the WiFi driver. This will not only start the driver but it
  // will also post WIFI_EVENT_STA_START to the default event loop. That's important because we want
  // to catch this event in our event handler and use it to start the WiFi connection attempt.
  ESP_ERROR_CHECK(esp_wifi_start() );

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
   * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
          pdFALSE,
          pdFALSE,
          portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT) {
      ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
               EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
  } else if (bits & WIFI_FAIL_BIT) {
      ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
               EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
  } else {
      ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }

  // Clean up ... unregister the events and delete the event group
  ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
  ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
  vEventGroupDelete(s_wifi_event_group);
}

void app_main()
{
  // Non Volatile Storage
  // Initialize NVS. We use NVS to store our WiFi configuration. That way the configuration
  // can be preserved across system boots. We also do some error checking to ensure that
  // if we do not first successfully initialize NVS that we try to erase the NVS and then 
  // try again.
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
  wifi_init_sta();

  // Create a new FreeRTOS task and add to the task list. The associated function
  // is udp_server_task (see above)) and we'll use 4096 words (NOT BYTES) for the 
  // task stack.  Let's use priority 5 for this task. Remember, tasks are infinite
  // loops; don't try to exit and don't try to return (but it is OK to delete the task).
  // See your above udp_server_task function for more details.
  xTaskReturn = xTaskCreate(udp_server_task,"udp_server",4096,NULL,5,NULL);

  if(xTaskReturn == pdPASS)
  {
    ESP_LOGI(TAG,"UDP server task started\n");
  }
}
