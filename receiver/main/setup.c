// setup.c
// Place all setup functions in this module. Please remember to add this module
// to the CMakeLists.txt file or it won't get compiled and linked! You'll also get "undefined
// reference" errors when compiling. 
//

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include "functions.h"

// These are defined via the menuconfig. Use idf.py menuconfig
#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD

// FreeRTOS event group to signal when we are connected
EventGroupHandle_t s_wifi_event_group;

// The event group allows multiple bits for each event, but we only care about two events:
//   - We are connected to the AP with an IP
//   - We failed to connect after the maximum amount of retries
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

extern const char *TAG;

// This is our WiFi setup function. It will cofigure the ESP32 WiFi in 2 steps: 1st
// step is the automatic configuration of various resources and 2nd step is configuration
// of specific parameters such as SSID and password. Then we setup the event handler (see functions.c).
// The event handler will detect the WiFi related events as they are posted to the default event loop.
// Once the WiFi is configured, it is started and log messages are printed to output or error.
void wifi_init_sta()
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
  //   - ESP_EVENT_ANY_ID: This is a "catch all" for alerting us to ANY of the various WIFI_EVENT events
  //   - IP_EVENT_STA_GOT_IP: The DHCP client on the ESP32 has received a valid IP address 
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
  ESP_LOGI(TAG, "Success wifi_init_sta finished!");

  // Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
  // number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see functions.c)
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
          pdFALSE,
          pdFALSE,
          portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT) {
      ESP_LOGI(TAG, "Connected to SSID:%s password:%s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
  } else if (bits & WIFI_FAIL_BIT) {
      ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
  } else {
      ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }

  // Clean up ... unregister the events and delete the event group
  ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
  ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
  vEventGroupDelete(s_wifi_event_group);
}
