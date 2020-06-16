// Configure ESP8266 to read the "open" or "closed" status of garage door via
// a tilt switch. See setup.c for details of breadboard connection. GPIO2 is
// used with the tilt switch. Send the status of the garage door to "receiver"
// every 5 seconds via a UDP datagram.

// Compile, link and then convert to a bin using the Makefile:  
//      make clean 
//      make
// Then flash to device using the Makefile.  Device must be in program mode:
//      make flash

// IMPORTANT: Remember that the ESP8266 NON-OS firmware does not contain an
// operating system. No OS means no task scheduler. This means we have to use
// some other means of putting our code on the SoC for execution. We'll use a timer.

#include "user_interface.h"
#include "osapi.h"
#include "gpio.h"
#include "espconn.h"
#include "user_config.h"
#include "debug.h"

LOCAL os_timer_t the_timer;
LOCAL struct espconn udp_espconn;

void ICACHE_FLASH_ATTR user_rf_pre_init(void)
{
}

uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:

            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

// Timer function 
LOCAL void ICACHE_FLASH_ATTR timer_function (void) {
  poll_function(&udp_espconn);
}

// This is the system init done callback function.
LOCAL void ICACHE_FLASH_ATTR init_done_callback(void) 
{
 
  uint16 voltage = 0;

  uart_div_modify(0, UART_CLK_FREQ / 115200);
  #ifdef DEBUG_ON
    os_printf("\n\nDEBUG_ON\n");
  #endif

  setup_gpio();
  setup_wifi();
  setup_udp(&udp_espconn);
  create_udp(&udp_espconn);

  // Display the system voltage.  Note that in order for this to work you HAVE to
  // set the 127th (last) byte of esp_init_data_default.bin to 0xFF (255) and
  // use esptool.py write_flash to write the new esp_init_data_default.bin to
  // 0xFC000.
  voltage = system_get_vdd33();

  #ifdef DEBUG_ON
    os_printf("System voltage: %d.%d\n", voltage / 1024, ((voltage%1024)*100)/1024);
  #endif

  // Setup the timer
  os_timer_disarm(&the_timer);
  os_timer_setfn(&the_timer, (os_timer_func_t *)timer_function, NULL);
  os_timer_arm(&the_timer, 5000, 1);

}

// Entry function ... execution starts here. Remember this ain't like regular
// c main function! Use the system init done callback function to do your
// setup AFTER the SoC has done all its initialization.
void ICACHE_FLASH_ATTR user_init (void) {
  
  system_init_done_cb(init_done_callback);

}
