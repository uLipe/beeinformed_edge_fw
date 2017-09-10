/*
 * app_ble.c
 *
 *  Created on: 01/06/2017
 *      Author: root
 */

#include "../usr_include/beeinformed.h"

/** task creation parameters */
#define BLE_APP_STK_SIZE			372
#define BLE_APP_PRIO				(configMAX_PRIORITIES - 3)
#define BLE_HCI_WORK_PRIO			(configMAX_PRIORITIES - 4)
#define BLE_HCI_HIGH_PRIO			(configMAX_PRIORITIES - 2)
#define BLE_QUEUE_SLOTS				(32)
#define BLE_DATA_SVC_MAX_SIZE		(20)
#define BLE_SEND_TIMEOUT			(5000)
#define BLE_RECV_TIMEOUT			(5000)



/** static variables */
static const  uint8_t ble_dev_name[] = "beeinformed_edge";
static uint8_t ble_incoming_packet[2 * sizeof(ble_data_t)] = {0};
static uint8_t *txpos = NULL;
static uint32_t data_remaining = 0;
static ble_data_t ble_tx_descriptor;
static bool ble_send_error = false;

/* sensor node data holders */
static uint16_t audio_buffer[AUDIO_SAMPLE_RATE] = {0};
static sensor_data_t sensor_frame;


static QueueHandle_t ble_rx_queue;
static SemaphoreHandle_t ble_rdy_sema;
static SemaphoreHandle_t ble_sent_sema;
static SemaphoreHandle_t ble_sent_signal;
static TimerHandle_t  ble_recv_timeout;



/**
 * @brief notification callback function
 */
static void ble_notify_cb(BLE_connectionDetails_t conn)
{
	/* process ccore stack global events */

	switch(conn.connectionStatus) {
		case BLE_CONNECTED_TO_DEVICE:
			printf(" %s: Device connected: %02x:%02x:%02x:%02x:%02x:%02x \r\n", __func__,
				 conn.remoteAddress.addr[0],
				 conn.remoteAddress.addr[1],
				 conn.remoteAddress.addr[2],
				 conn.remoteAddress.addr[3],
				 conn.remoteAddress.addr[4],
				 conn.remoteAddress.addr[5]);
			break;

		case BLE_DISCONNECTED_FROM_DEVICE:
			printf("%s: Device disconnected!\r\n", __func__);
			break;

		default:
			break;
	}

}

/**
 * ble_recv_timeout_cb()
 * @brief handles packet reception timeout
 * @param
 * @reuturn
 */
static void ble_recv_timeout_cb(TimerHandle_t *t)
{
	(void)t;
	printf("%s: Packet reception timeout! \n\r", __func__);
}


/**
 * @brief callback related to alpwise data exch svc
 */
static void ble_alpw_data_xch_cb(BleAlpwDataExchangeEvent ev, BleStatus sts,
			void *params)	{
	if((ev == BLEALPWDATAEXCHANGE_EVENT_RXDATA) &&
				(sts == BLESTATUS_SUCCESS) && (params != NULL)) {

		PTD_pinOutToggle(PTD_PORT_LED_ORANGE,PTD_PIN_LED_ORANGE);


		/* extract data arrived from client */
		BleAlpwDataExchangeServerRxData *rx =
				(BleAlpwDataExchangeServerRxData *) params;

		/* fill the packet */
		memcpy(&ble_incoming_packet[0], rx->rxData, rx->rxDataLen);
		printf("%s: Raw data arrived len: %d! \n\r", __func__, rx->rxDataLen);
		xQueueSend(ble_rx_queue, &ble_incoming_packet, 0);
		PTD_pinOutToggle(PTD_PORT_LED_ORANGE,PTD_PIN_LED_ORANGE);
	}

	/* acknowledge that  data was sent */
	else if(ev == BLEALPWDATAEXCHANGE_EVENT_TXCOMPLETE) {
		PTD_pinOutToggle(PTD_PORT_LED_YELLOW,PTD_PIN_LED_YELLOW);

		/* advances to next packet */
		data_remaining -= ble_tx_descriptor.payload_size;
		txpos+= ble_tx_descriptor.payload_size;

		if(data_remaining) {


			ble_tx_descriptor.payload_size = (data_remaining >= PACKET_MAX_PAYLOAD) ?
											  PACKET_MAX_PAYLOAD 					 :
											  data_remaining;

			/*
			 * If data has been splitten in more than 1 packets
			 * the further one will carry a special type called
			 * sequence with 0 packet number, which can be used
			 * by client to identify out of sync transmission
			 */
			ble_tx_descriptor.type = k_sequence_packet;
			ble_tx_descriptor.pack_amount = 0;

			if(BLE_sendData((uint8_t *)&ble_tx_descriptor, sizeof(ble_data_t)) !=
					BLE_SUCCESS	) {

				/* failed to send, reset env */
				data_remaining = 0;
				txpos = 0;
				printf("%s: failed to send packet  \n\r", __func__);
				ble_send_error = true;
				xSemaphoreGive(ble_sent_signal);

			}
		} else {
			printf("%s: complete packet transmitted! \n\r", __func__);
			xSemaphoreGive(ble_sent_signal);
		}
		PTD_pinOutToggle(PTD_PORT_LED_YELLOW,PTD_PIN_LED_YELLOW);
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
static void ble_send_packet(ble_data_t *b,  uint8_t *data, uint32_t noof_packets)
{
	xSemaphoreTake(ble_sent_sema, portMAX_DELAY);

	if(b == NULL)
		goto cleanup;

	if(data == NULL)
		goto cleanup;

	if(noof_packets == 0)
		goto cleanup;

	if(noof_packets > (255 * PACKET_MAX_PAYLOAD) )
		goto cleanup;


	/* protect from race condition */
	if(1) {
		/* prepare for transmission */
		memcpy(&ble_tx_descriptor, b, sizeof(ble_data_t));
		txpos = data;
		data_remaining = noof_packets;
		ble_tx_descriptor.pack_amount = data_remaining;
		printf("%s: sending data through ble! \n\r type: %u \n\r id: %u \n\r packs: %u \n\r",
				__func__,
				ble_tx_descriptor.type,
				ble_tx_descriptor.id,
				ble_tx_descriptor.pack_amount);

		/* round packets qty up to avoid loss of data out of
		 * PACKET_MAX_PAYLOAD boundary
		 */
		if((noof_packets % PACKET_MAX_PAYLOAD) == 0) {
			ble_tx_descriptor.pack_amount = (noof_packets / (PACKET_MAX_PAYLOAD));
		} else {
			ble_tx_descriptor.pack_amount = (noof_packets / (PACKET_MAX_PAYLOAD)) + 1;
		}

		ble_tx_descriptor.payload_size = noof_packets < PACKET_MAX_PAYLOAD ?
														noof_packets :
														PACKET_MAX_PAYLOAD;

		memcpy(&ble_tx_descriptor.pack_data, txpos, PACKET_MAX_PAYLOAD);
		if(BLE_sendData((uint8_t *)&ble_tx_descriptor, sizeof(ble_data_t)) != BLE_SUCCESS) {
			printf("%s: failed to transmit, please retry!", __func__);
			goto cleanup;

		} else {
			/* wait until all packets goes off BLE buffer */
			xSemaphoreTake(ble_sent_signal, portMAX_DELAY);


			printf("%s: done!", __func__);
		}


	}

cleanup:
	/* Give mutex for next requester */
	xSemaphoreGive(ble_sent_sema);
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
 * func()
 * @brief
 * @param
 * @reuturn
 */
static void ble_on_sensor_acquired(sensor_data_t *d, sensor_status_t s, void *user_data)
{
	ble_data_t reply;

	reply.type = k_data_packet;
	reply.id   = (edge_cmds_t) user_data;

	if(s == ksensor_ok) {
		ble_send_packet(&reply, (uint8_t *)d, sizeof(sensor_data_t));
	}
}

/**
 * func()
 * @brief
 * @param
 * @reuturn
 */
static void ble_on_audio_acquired(void *user_data, audio_status_t s)
{
	ble_data_t reply;

	reply.type = k_data_packet;
	reply.id   = (edge_cmds_t) user_data;

	if(s == kaudio_acquisition_ok) {
		ble_send_packet(&reply, (uint8_t *)audio_buffer, 255);
	}
}


/**
 * handle_sensors()
 * @brief handles the sensor readings request from client
 * @param
 * @reuturn
 */
static inline void handle_sensors(ble_data_t *b)
{
	(void )b;
	sensor_trigger_reading(&sensor_frame, ble_on_sensor_acquired, &b->id);
}


/**
 * handle_audio()
 * @brief
 * @param
 * @reuturn
 */
static inline void handle_audio(ble_data_t *b)
{
	/* for audio the process to send data is a bit more complex
	 * first we need to know how much packets we need to send
	 */
	(void)b;

	/* start audio capture */
	audio_start_record(&audio_buffer[0], 1, ble_on_audio_acquired, &b->id);
}

/**
 * @brief bt packets command handler
 */
static void ble_cmd_handler(ble_data_t *b)
{
	edge_cmds_t cmd = (edge_cmds_t)b->id;

	switch(cmd) {
	case k_get_sensors:
		printf("%s: Client is requesting luminosity over BLE! \n\r", __func__);
		handle_sensors(b);
		break;

	case k_get_audio:
		printf("%s: Client is requesting audio over BLE! \n\r", __func__);
		handle_audio(b);
		break;
	default:
		printf("%s: Unknown command, packet will not processed! \n\r", __func__);
		break;
	}
}

static void ble_hci_task(void *args)
{
	(void)args;

	for(;;) {
		/* run the HCI and core processing stack */
		BLE_hciReceiveData();
	}
}


/**
 * @brief main ble application task
 */
static void ble_app_task(void *args)
{
	(void)args;
	ble_recv_timeout = xTimerCreate("ble_recv_timer", BLE_RECV_TIMEOUT, pdFALSE,
							NULL, ble_recv_timeout_cb);


	/* waits until bt stack core is ready */
	xSemaphoreTake(ble_rdy_sema, portMAX_DELAY);
	printf("%s: bt core stack signaled! bt app is ready! \n\r", __func__);
	printf("%s: System Clock is: %lu [Hz]! \n\r", __func__, SystemCoreClock);

	/* starts the audio capturing */
    audio_app_init();
    /* temperature task init */
    sensor_app_start();

	for(;;) {
		/* blocks until a message is avalialble */
		ble_data_t packet;
		xQueueReceive(ble_rx_queue, &packet, portMAX_DELAY);

		printf("%s: packet contents are:  \n\r", __func__);
		printf("%s: type: 0x%X \n\r", __func__, packet.type);
		printf("%s: id: 0x%X \n\r", __func__, packet.id);


		if(packet.type != k_command_packet) {
			/* only command packets are allowed to process
			 * by edges, otherwise, force a id corruption
			 * and discard packet
			 */
			packet.id = 0xFF;
		}
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

	ble_sent_signal = xSemaphoreCreateBinary();
	assert(ble_sent_signal != NULL);

	ble_sent_sema = xSemaphoreCreateMutex();
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
