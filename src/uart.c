#include <zephyr.h>
#include <stdio.h>
#include <drivers/uart.h>
#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <SEGGER_RTT.h>
#include "UART_handler.h"
#include "uart_entry.h"
#include "udp.h"

void printk_uint8_t(uint8_t *arr, uint8_t len)
{
	for (int i = 0; i < len; i++) {
		printk("%d", arr[i]);
		printk("%s", "-");
	}
	printk("\n");
}

const struct device *uart_div;

uint8_t get_rx_buf_size(uint8_t *rx_buf)
{
	for (int i = 0; i < 256; i++) {
		if ((rx_buf[i] == 102) && (rx_buf[i + 1] == 105) &&
		    (rx_buf[i + 2] == 110) && (rx_buf[i + 3] == 105) &&
		    (rx_buf[i + 4] == 115) && (rx_buf[i + 5] == 104) &&
		    (rx_buf[i + 6] == 49) && (rx_buf[i + 7] == 48) &&
		    (rx_buf[i + 8] == 50)) {
			return i;
		}
	}
	return 0;
}

void uart_entry(void)
{
	// uart handle
	SEGGER_RTT_Init();
	k_msleep(3000);
	printk("uart_entry started \n");
	uart_div = init_uart_dev("UART_0");
	if (!uart_div) {
		printk("Failed to initialize UART Device");
	}

	bool uart_enabled = false;
	int len;
	int err;
	char tx_buf[UART_TX_LINE_BUF_SIZE] = { '\0' };
	uint8_t rx_buf[UART_RX_LINE_BUF_SIZE] = { '\0' };
	while (1) {
		printk("waiting to receive data... \n");
		if (!uart_enabled) {
			uart_enabled = true;
			uart_irq_rx_enable(uart_div);
		}
		//Wait on UART receiption forever...
		len = rx_message_uart(rx_buf, K_FOREVER, uart_div);
		// printk("rx_len %d\n", len);
		if (len <= 0) {
			printk("Failed UART RX Receiption\n");
		} else {
			printk("data recived: %s", rx_buf);
			//Notify D9x11 tEhat message was received
			if (server_connected && modem_status) {
				snprintf(tx_buf, UART_TX_LINE_BUF_SIZE,
					 "ACK\n");
			} else {
				snprintf(tx_buf, UART_TX_LINE_BUF_SIZE,
					 "WAITING_FOR_CONNECTION\n");
			}
			err = tx_message_uart(tx_buf, strlen(tx_buf), uart_div);
			if (err < 0) {
				printk("FAILED UART TX\n");
			}
			memset(tx_buf, 0, sizeof(tx_buf));
			//Print message for debug purposes
			printk("rx_buf:%s :\n", rx_buf);
			len = get_rx_buf_size(rx_buf);
			// printk("rx_len calc %d\n", len);
			data.size = len;
			memcpy(data.buff, rx_buf, len);

			data.state = DATA_PENDING;
			memset(rx_buf, 0, sizeof(rx_buf));
			// printk_uint8_t(data.buff, data.size);
		}
	}
}

K_THREAD_DEFINE(uart_entry_id, STACKSIZE, uart_entry, NULL, NULL, NULL, 2, 0,
		0);
