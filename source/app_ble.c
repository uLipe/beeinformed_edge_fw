/*
 * app_ble.c
 *
 *  Created on: 01/06/2017
 *      Author: root
 */

#include "../usr_include/beeinformed.h"

/** task creation parameters */
#define BLE_APP_STK_SIZE			512
#define BLE_APP_PRIO				(configMAX_PRIORITIES - 3)
#define BLE_HCI_WORK_PRIO			(configMAX_PRIORITIES - 4)
#define BLE_HCI_HIGH_PRIO			(configMAX_PRIORITIES - 2)
#define BLE_QUEUE_SLOTS				(8)
#define BLE_DATA_SVC_MAX_SIZE		(20)
#define BLE_SEND_TIMEOUT			(2000)

/** static variables */
static const  uint8_t ble_dev_name[] = "ulipe_xdk_ble";
static uint8_t ble_incoming_packet[sizeof(ble_data_t)] = {0};
static uint8_t ble_outcoming_packet[sizeof(ble_data_t)] = {0};
static uint8_t pos = 0;
static uint8_t txpos = 0;
static uint8_t data_remaining = 0;

static QueueHandle_t ble_rx_queue;
static SemaphoreHandle_t ble_rdy_sema;
static SemaphoreHandle_t ble_sent_sema;




/**
 * @brief notification callback function
 */
static void ble_notify_cb(BLE_connectionDetails_t conn)
{
	/* process ccore stack global events */

	switch(conn.connectionStatus) {
		case BLE_CONNECTED_TO_DEVICE:
			printf( "Device connected: %02x:%02x:%02x:%02x:%02x:%02x \r\n",
				 conn.remoteAddress.addr[0],
				 conn.remoteAddress.addr[1],
				 conn.remoteAddress.addr[2],
				 conn.remoteAddress.addr[3],
				 conn.remoteAddress.addr[4],
				 conn.remoteAddress.addr[5]);
			break;

		case BLE_DISCONNECTED_FROM_DEVICE:
			printf("Device disconnected!\r\n");
			break;


		default:
			break;
	}

}

/**
 * @brief callback related to alpwise data exch svc
 */
static void ble_alpw_data_xch_cb(BleAlpwDataExchangeEvent ev, BleStatus sts,
			void *params)	{
	if((ev == BLEALPWDATAEXCHANGE_EVENT_RXDATA) &&
				(sts == BLESTATUS_SUCCESS) && (params != NULL)) {
		/* extract data arrived from client */
		BleAlpwDataExchangeServerRxData *rx =
				(BleAlpwDataExchangeServerRxData *) params;

		/* fill the packet */
		memcpy(&ble_incoming_packet[pos], rx->rxData, rx->rxDataLen);
		pos += rx->rxDataLen;
		if(pos >= sizeof(ble_data_t)) {
			/* send packet to app task */
			pos = 0;
			xQueueSend(ble_rx_queue, &ble_incoming_packet, 0);
			printf("%s: complete packet arrived! \n\r", __func__);
		}
	}

	/* acknowledge that  data was sent */
	else if(ev == BLEALPWDATAEXCHANGE_EVENT_TXCOMPLETE) {
		printf("%s : txpos: %d \n\r", __func__, txpos);

		if(data_remaining) {


			uint8_t size = (data_remaining >= BLE_DATA_SVC_MAX_SIZE) ?
							BLE_DATA_SVC_MAX_SIZE 					 :
							data_remaining;

			data_remaining -= size;
			txpos +=size;

			if(BLE_sendData(&ble_outcoming_packet[txpos], size) !=
					BLE_SUCCESS	) {
				/* failed to send, reset env */
				data_remaining = 0;
				txpos = 0;
				printf("%s: failed to send packet  \n\r", __func__);
				xSemaphoreGive(ble_sent_sema);

			}

			/* if was last packet ends transmission */
			if(size < BLE_DATA_SVC_MAX_SIZE) {
				data_remaining = 0;
				printf("%s: complete packet transmitted! \n\r", __func__);
				xSemaphoreGive(ble_sent_sema);
			}
		} else {
			printf("%s: complete packet transmitted! \n\r", __func__);
			xSemaphoreGive(ble_sent_sema);
		}
		printf("%s : data_remaining: %d \n\r", __func__, data_remaining);
	}
}

/**
 * @brief service registration callback
 */
static void ble_app_service_register_cb (void)
{
	BleStatus svc_status;
	svc_status = BLEALPWDATAEXCHANGE_SERVER_Register(ble_alpw_data_xch_cb);
	assert(svc_status == BLESTATUS_SUCCESS);
	xSemaphoreGive(ble_rdy_sema);
}

/**
 * @brief sends a packet via BLE interface
 */
static void ble_send_packet(ble_data_t *b)
{
	if(b == NULL)
		goto cleanup;

	memcpy(&ble_outcoming_packet, b, sizeof(ble_data_t));
	txpos = 0;
	data_remaining = sizeof(ble_data_t) - BLE_DATA_SVC_MAX_SIZE;

	BleStatus sts = BLE_sendData(&ble_outcoming_packet[0], BLE_DATA_SVC_MAX_SIZE);
	assert(sts != BLESTATUS_FAILED);

	portBASE_TYPE err = xSemaphoreTake(ble_sent_sema, BLE_SEND_TIMEOUT);
	if(err != pdTRUE) {
		/* reset packet sending environment */
		printf("%s: Timeout!\n\r", __func__);
		txpos = 0;
		data_remaining = 0;
	}
cleanup:
	return;
}



/**
 * @brief initializes the ble stack
 */
static int ble_init(void)
{
	int err = 0;

	BLE_notification_t cfg_params;
	BLE_returnStatus_t ble_ret;

	/* prepare ble parameters */
	cfg_params.callback = ble_notify_cb;
	cfg_params.enableNotification = BLE_ENABLE_NOTIFICATION;
	ble_ret = BLE_enablenotificationForConnect(cfg_params);
	assert(ble_ret != BLE_INVALID_PARAMETER);

	/* register data exchange service */
	BLE_status sts = BLE_customServiceRegistry(ble_app_service_register_cb);
	assert(sts != BLESTATUS_FAILED);


	/* sets the device name */
	ble_ret = BLE_setDeviceName((uint8_t *)ble_dev_name, sizeof(ble_dev_name));
	assert(ble_ret != BLE_INVALID_PARAMETER);

	/* initializes ble core stack */
	sts = BLE_coreStackInit();
	assert(sts != BLESTATUS_FAILED);

	return(err);
}

/**
 * @brief bt packets command handler
 */
static void ble_cmd_handler(ble_data_t *b)
{
	printf("%s: New command sent by client, processing! \n\r", __func__);
	ble_send_packet(b);
}

static void ble_hci_task(void *args)
{
	(void)args;

	for(;;) {
		/* run the HCI and core processing stack */
		BLE_return_t hci_err = BLE_hciReceiveData();
		if(hci_err != BLE_STATUS_SUCCESS) {
			/* if no data pending reduces the hci task priority */
			vTaskPrioritySet(xTaskGetCurrentTaskHandle(), BLE_HCI_WORK_PRIO);
			//printf("%s: no activity on hci bus! \n\r", __func__);
		} else {
			/* new data arrived, so arises the task priority */
			vTaskPrioritySet(xTaskGetCurrentTaskHandle(), BLE_HCI_HIGH_PRIO);
			//printf("%s: new activity on hci bus! \n\r", __func__);
		}

		BLE_coreStateMachine();
	}
}


/**
 * @brief main ble application task
 */
static void ble_app_task(void *args)
{
	(void)args;

	/* waits until bt stack core is ready */
	xSemaphoreTake(ble_rdy_sema, portMAX_DELAY);
	printf("%s: bt core stack signaled! bt app is ready! \n\r", __func__);

	for(;;) {
		/* blocks until a message is avalialble */
		ble_data_t packet;
		xQueueReceive(ble_rx_queue, &packet, portMAX_DELAY);
		ble_cmd_handler(&packet);
	}
}


/*
 * Public functions
 */

void app_ble_init(void)
{
	portBASE_TYPE err;

	/* creates semaphore to wait stack ready */
	ble_rdy_sema = xSemaphoreCreateBinary();
	assert(ble_rdy_sema != NULL);

	ble_sent_sema = xSemaphoreCreateBinary();
	assert(ble_sent_sema != NULL);

	/* create queue/timers for communication channel */
	ble_rx_queue = xQueueCreate(BLE_QUEUE_SLOTS, sizeof(ble_data_t));
	assert(ble_rx_queue != NULL);

	/* inits the bt stack */
	ble_init();

	err = xTaskCreate(ble_app_task,"ble_app",BLE_APP_STK_SIZE,NULL,BLE_APP_PRIO,NULL);
	assert(err == pdPASS);


	/* create the ble task */
	err = xTaskCreate(ble_hci_task,"ble_hci",BLE_APP_STK_SIZE,NULL,BLE_HCI_HIGH_PRIO,NULL);
	assert(err == pdPASS);
}
