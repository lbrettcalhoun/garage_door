#ifndef __USER_CONFIG__H

  #define __USER_CONFIG__H

  #include "user_interface.h"
  #include "espconn.h"

  void create_udp(struct espconn *p_espconn);
  void poll_function (struct espconn *p_espconn);
  void receive_callback(void *arg, char *p_data, unsigned short len);
  void sent_callback(void *arg);
  void setup_gpio (void);
  void setup_udp(struct espconn *p_espconn);
  void setup_wifi (void);

#endif
