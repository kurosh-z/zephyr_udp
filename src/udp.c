/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr.h>
#include <kernel.h>
#include <modem/lte_lc.h>
#include <net/socket.h>
#include <modem/modem_info.h>
#include <modem/at_params.h>
#include <modem/at_cmd.h>
#include <cJSON.h>
#include <cJSON_os.h>
#include <stdio.h>
#include <SEGGER_RTT.h>
#include <net/socket.h>
#include <stdio.h>
#include <zephyr.h>
#include <string.h>
#include <drivers/uart.h>
#include <udp.h>

static int client_fd;
static struct sockaddr_storage host_addr;
static struct k_work_delayable server_transmission_work;

uint8_t server_connected = SERV_PENDING;
uint8_t modem_status = MODEM_PENDING;
// struct Data data = { .buff = "COLL_test123",
// 		     .state = DATA_PENDING,
// 		     .size = 12 };
struct Data data = {
	.buff = { 10,  104, 10,	 32,  8,   0,	18,  9,	  13,  0,   192, 45,
		  197, 16,  0,	 24,  2,   24,	215, 218, 155, 143, 6,	 34,
		  3,   99,  104, 52,  40,  3,	50,  4,	  8,   0,   16,	 0,
		  10,  33,  8,	 2,   18,  9,	13,  0,	  128, 228, 196, 16,
		  0,   24,  2,	 24,  215, 218, 155, 143, 6,   34,  4,	 99,
		  104, 52,  43,	 40,  3,   50,	4,   8,	  0,   16,  0,	 10,
		  31,  8,   4,	 18,  9,   13,	0,   0,	  0,   0,   16,	 1,
		  24,  2,   24,	 215, 218, 155, 143, 6,	  34,  2,   67,	 79,
		  40,  3,   50,	 4,   8,   0,	16,  0,	  16,  0 },
	.state = DATA_PENDING,
	.size = 106
};

K_SEM_DEFINE(lte_connected, 0, 1);

static void server_transmission_work_fn(struct k_work *work)
{
	int err;
	// printk("sending data:\n");
	// char buffer[CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES] = { "testing123" };
	if (data.state == DATA_PENDING) {
		printk("Transmitting UDP/IP payload of %d bytes to the \n",
		       CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES + UDP_IP_HEADER_SIZE);
		// printk("IP address %s, port number %d\n",
		//        CONFIG_UDP_SERVER_ADDRESS_STATIC,
		//        CONFIG_UDP_SERVER_PORT);

		// err = send(client_fd, data.buff, sizeof(data.buff), 0);
		err = send(client_fd, data.buff, data.size, 0);
		if (err < 0) {
			printk("Failed to transmit UDP packet, %d\n", errno);
			return;
		}

		// data.state = DATA_SENT;
	}
	// K_SECONDS(CONFIG_UDP_DATA_UPLOAD_FREQUENCY_SECONDS);
	// k_work_schedule(&server_transmission_work, K_MSEC(10));
	k_work_schedule(&server_transmission_work, K_MSEC(10000));

	// k_work_schedule(&server_transmission_work, K_NO_WAIT);
}

static void work_init(void)
{
	k_work_init_delayable(&server_transmission_work,
			      server_transmission_work_fn);
}

#if defined(CONFIG_NRF_MODEM_LIB)
static void lte_handler(const struct lte_lc_evt *const evt)
{
	// printk("from lte_handler \n");
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:

		if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
		    (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING)) {
			if (evt->nw_reg_status == LTE_LC_NW_REG_UICC_FAIL) {
				printk("No SIM card detected!\n");
			}
			break;
		}

		printk("Network registration status: %s\n",
		       evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ?
				     "Connected - home network" :
				     "Connected - roaming\n");

		k_sem_give(&lte_connected);
		break;
	case LTE_LC_EVT_PSM_UPDATE:
		printk("PSM parameter update: TAU: %d, Active time: %d\n",
		       evt->psm_cfg.tau, evt->psm_cfg.active_time);
		break;
	case LTE_LC_EVT_EDRX_UPDATE: {
		// printk("3.case \n");
		char log_buf[60];
		ssize_t len;

		len = snprintf(log_buf, sizeof(log_buf),
			       "eDRX parameter update: eDRX: %f, PTW: %f\n",
			       evt->edrx_cfg.edrx, evt->edrx_cfg.ptw);
		if (len > 0) {
			printk("%s\n", log_buf);
		}
		break;
	}
	case LTE_LC_EVT_RRC_UPDATE:
		printk("RRC mode: %s\n",
		       evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ?
				     "Connected" :
				     "Idle\n");
		break;
	case LTE_LC_EVT_CELL_UPDATE:
		printk("LTE cell changed: Cell ID: %d, Tracking area: %d\n",
		       evt->cell.id, evt->cell.tac);
		if (evt->cell.id == -1) {
			modem_status = MODEM_PENDING;
		} else {
			modem_status = MODEM_CONNECTED;
		}
		break;
	default:
		// printk("default case \n");
		break;
	}
}

static int configure_low_power(void)
{
	int err;

#if defined(CONFIG_UDP_PSM_ENABLE)
	/** Power Saving Mode */
	err = lte_lc_psm_req(true);
	if (err) {
		printk("lte_lc_psm_req, error: %d\n", err);
	}
#else
	err = lte_lc_psm_req(false);
	if (err) {
		printk("lte_lc_psm_req, error: %d\n", err);
	}
#endif

#if defined(CONFIG_UDP_EDRX_ENABLE)
	/** enhanced Discontinuous Reception */
	err = lte_lc_edrx_req(true);
	if (err) {
		printk("lte_lc_edrx_req, error: %d\n", err);
	}
#else
	err = lte_lc_edrx_req(false);
	if (err) {
		printk("lte_lc_edrx_req, error: %d\n", err);
	}
#endif

#if defined(CONFIG_UDP_RAI_ENABLE)
	/** Release Assistance Indication  */
	err = lte_lc_rai_req(true);
	if (err) {
		printk("lte_lc_rai_req, error: %d\n", err);
	}
#endif

	return err;
}

static void modem_init(void)
{
	int err;

	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already configured and LTE connected. */
	} else {
		err = lte_lc_init();
		if (err) {
			printk("Modem initialization failed, error: %d\n", err);
			return;
		}
	}
}

static void modem_connect(void)
{
	int err;

	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already configured and LTE connected. */
	} else {
		err = lte_lc_connect_async(lte_handler);
		printk("Connecting to LTE network, error: %d\n", err);
		if (err) {
			printk("Connecting to LTE network failed, error: %d\n",
			       err);
			return;
		}
	}
}
#endif

static void server_disconnect(void)
{
	(void)close(client_fd);
}

static int server_init(void)
{
	struct sockaddr_in *server4 = ((struct sockaddr_in *)&host_addr);

	server4->sin_family = AF_INET;
	server4->sin_port = htons(CONFIG_UDP_SERVER_PORT);

	inet_pton(AF_INET, CONFIG_UDP_SERVER_ADDRESS_STATIC,
		  &server4->sin_addr);

	return 0;
}

static int server_connect(void)
{
	int err;

	client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (client_fd < 0) {
		printk("Failed to create UDP socket: %d\n", errno);
		err = -errno;
		goto error;
	}

	err = connect(client_fd, (struct sockaddr *)&host_addr,
		      sizeof(struct sockaddr_in));
	if (err < 0) {
		printk("Connect failed : %d\n", errno);
		goto error;
	}

	return 0;

error:
	server_disconnect();

	return err;
}

void udp_entry()
{
	int err;

	SEGGER_RTT_Init();
	printk("UDP has started\n");
	work_init();
	// printk("after work_init\n");
#if defined(CONFIG_NRF_MODEM_LIB)

	/* Initialize the modem before calling configure_low_power(). This is
   * because the enabling of RAI is dependent on the
   * configured network mode which is set during modem initialization.
   */
	modem_init();

	err = configure_low_power();
	if (err) {
		printk("Unable to set low power configuration, error: %d\n",
		       err);
	}
	printk(" modem_low_power configured!\n");
	modem_connect();
	printk(" starting modem connection ...\n");
	k_sem_take(&lte_connected, K_FOREVER);
	modem_status = MODEM_CONNECTED;
	printk("lte_connected!\n");
#endif

	err = server_init();

	if (err) {
		printk("Not able to initialize UDP server connection\n");
		return;
	}

	err = server_connect();
	if (err) {
		printk("Not able to connect to UDP server\n");
		return;
	}
	server_connected = SERV_CONNECTED;
	k_work_schedule(&server_transmission_work, K_NO_WAIT);
}

// K_THREAD_DEFINE(udp_id, STACKSIZE, udp, NULL, NULL, NULL, 3, 0, 0);

// K_THREAD_STACK_DEFINE(_k_thread_stack_udp, STACKSIZE);

// struct k_thread udp_thread;
// k_tid_t my_tid = k_thread_create(&udp_thread, _k_thread_stack_udp, STACKSIZE,
// 				 udp_entry, NULL, NULL, NULL, 3, 0, K_NO_WAIT);
static K_THREAD_STACK_DEFINE(udp_stack_area, STACKSIZE);
static struct k_thread udp_thread_data;
k_tid_t udp_t_id;
int start_udp_thread(void)
{
	udp_t_id =
		k_thread_create(&udp_thread_data, udp_stack_area,
				K_THREAD_STACK_SIZEOF(udp_stack_area),
				udp_entry, NULL, NULL, NULL, 3, 0, K_NO_WAIT);

	return 0;
}

int main()
{
	uint32_t current_time = k_uptime_get_32();
	start_udp_thread();
	uint32_t start_time = k_uptime_get_32();
	while (1) {
		while (modem_status == MODEM_PENDING ||
		       server_connected == SERV_PENDING) {
			k_msleep(20000); //wait another 20 sec
			current_time = k_uptime_get_32();
			if (current_time - start_time >
			    MODEM_MAX_WAIT_TIME_MS) {
				printk("\n\n-------------------------\n");
				printk("restarting upd_entry \n");
				printk("\n\n-------------------------\n");
				k_thread_abort(&udp_thread_data);
				k_msleep(3000);
				start_udp_thread();
				start_time = k_uptime_get_32();
			}
		}
		//check after a while if modem is still connected
		k_msleep(MODEM_FREQUENT_CHECK_TIME_MS);
	}
}