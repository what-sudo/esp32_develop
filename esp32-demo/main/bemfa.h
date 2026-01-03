#ifndef __BEMFA_H__
#define __BEMFA_H__

#define BEMFA_DEVICE_ADDTOPIC_API    "http://pro.bemfa.com/vs/web/v1/deviceAddTopic"

void user_bemfa_connect_task(void *pvParameters);

int parse_bemfa_bind_message(char *rx_buf, char *tx_buf);

#endif
