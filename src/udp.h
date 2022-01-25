#ifndef UDP_SEND_H_
#define UDP_SEND_H_

#define STACKSIZE 2048
#define UDP_IP_HEADER_SIZE 28
#define DATA_PENDING 0
#define DATA_SENT 1

#define MODEM_CONNECTED 1
#define MODEM_PENDING 0
#define MODEM_MAX_WAIT_TIME_MS 90000
#define MODEM_FREQUENT_CHECK_TIME_MS 15000
#define SERV_CONNECTED 1
#define SERV_PENDING 0

typedef struct Data {
	uint8_t buff[CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES];
	uint8_t state;
	uint8_t size;
} Data;
extern struct Data data;
extern uint8_t server_connected;
extern uint8_t modem_status;

int start_udp_thread(void);

#endif