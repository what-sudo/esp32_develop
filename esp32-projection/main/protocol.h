#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

void udp_server_task(void *pvParameters);

int dns_lookup(const char *hostname, char *ip_address);

int tcp_client_init(char *ip_addr, int port);
int tcp_client_deinit(int sock);

#endif
