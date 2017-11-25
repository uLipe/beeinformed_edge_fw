/*
 * app_ble.c
 *
 *  Created on: 01/06/2017
 *      Author: root
 */

#include "../usr_include/beeinformed.h"

/** task creation parameters */
#define BLE_APP_STK_SIZE			372
#define BLE_APP_PRIO				(14)
#define BLE_QUEUE_SLOTS				(32)
#define BLE_DATA_SVC_MAX_SIZE		(20)
#define BLE_SEND_TIMEOUT			(5000)
#define BLE_RECV_TIMEOUT			(5000)
#define BLE_START_SYNC_TIMEOUT		(5000)
#define BLE_WAKEUP_SYNC_TIMEOUT 	(5000)



/** static variables */
static const  uint8_t ble_dev_name[] = "beeinformed_edge";
static uint8_t ble_incoming_packet[2 * sizeof(ble_data_t)] = {0};
static uint8_t *txpos = NULL;
static uint32_t data_remaining = 0;
static ble_data_t ble_tx_descriptor;
static bool ble_send_error = false;

/* sensor node data holders */
AUDIO_CREATE_BUFFER(audio_buffer, BLE_APP_AUDIO_CAPTURE_PERIOD);
static sensor_data_t sensor_frame;


static QueueHandle_t ble_rx_queue;
static SemaphoreHandle_t ble_sent_sema;
static SemaphoreHandle_t ble_sent_signal;
static TimerHandle_t  ble_recv_timeout;

static SemaphoreHandle_t BleStartSyncSemphr = NULL;
static SemaphoreHandle_t BleWakeUpSyncSemphr = NULL;

/**
 * @brief notification callback function
 */
static void ble_notify_cb(BlePeripheral_Event_T event, void *data)
{
	/* process ccore stack global events */

	switch(event) {
	case BLE_PERIPHERAL_STARTED:
		xSemaphoreGive( BleStartSyncSemphr );
		break;

	case BLE_PERIPHERAL_WAKEUP_SUCCEEDED:
		xSemaphoreGive( BleWakeUpSyncSemphr );
		break;

	case BLE_PERIPHERAL_CONNECTED:
	{
		Ble_RemoteDeviceAddress_T *remoteAddress;
		remoteAddress = (Ble_RemoteDeviceAddress_T*) data;
		printf("Device connected: %02x:%02x:%02x:%02x:%02x:%02x\n\r",
					remoteAddress->Addr[0], remoteAddress->Addr[1],
					remoteAddress->Addr[2], remoteAddress->Addr[3],
					remoteAddress->Addr[4], remoteAddress->Addr[5]);
		break;
	}

	case BLE_PERIPHERAL_DISCONNECTED:
		printf("Device Disconnected\n\r");
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

static void ble_data_rx_cb(uint8_t *data, uint8_t len) {

	BSP_LED_Switch(BSP_XDK_LED_O, BSP_LED_COMMAND_ON);


	/* fill the packet */
	memcpy(&ble_incoming_packet[0], data, len);
	printf("%s: Raw data arrived len: %d! \n\r", __func__, len);
	xQueueSend(ble_rx_queue, &ble_incoming_packet, 0);

	BSP_LED_Switch(BSP_XDK_LED_O, BSP_LED_COMMAND_OFF);

}



static void ble_data_tx_cb(Retcode_T err) {
	BCDS_UNUSED(err);

	BSP_LED_Switch(BSP_XDK_LED_Y, BSP_LED_COMMAND_ON);

	/* advances to next packet */
	data_remaining -= ble_tx_descriptor.payload_size;
	txpos+= ble_tx_descriptor.payload_size * sizeof(uint8_t);

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

		memcpy(&ble_tx_descriptor.pack_data, txpos, ble_tx_descriptor.payload_size * sizeof(uint8_t));


		if(BidirectionalService_SendData((uint8_t *)&ble_tx_descriptor, sizeof(ble_data_t)) !=
				RETCODE_OK	) {

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

	BSP_LED_Switch(BSP_XDK_LED_Y, BSP_LED_COMMAND_OFF);

}



/**
 * @brief service registration callback
 */
static Retcode_T ble_app_service_register_cb (void)
{
	BidirectionalService_Init(ble_data_rx_cb, ble_data_tx_cb);
	BidirectionalService_Register();
	return (RETCODE_OK);
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

		memcpy(&ble_tx_descriptor.pack_data, txpos, PACKET_MAX_PAYLOAD * sizeof(uint8_t));

		if(BidirectionalService_SendData((uint8_t *)&ble_tx_descriptor, sizeof(ble_data_t)) != RETCODE_OK) {
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
	audio_start_record(&audio_buffer[0], AUDIO_BUFFER_GET_LEN_IN_MS(audio_buffer), ble_on_audio_acquired, &b->id);

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

/**
 * @brief main ble application task
 */
static void ble_app_task(void *args)
{
	(void)args;
	ble_recv_timeout = xTimerCreate("ble_recv_timer", BLE_RECV_TIMEOUT, pdFALSE,
							NULL, ble_recv_timeout_cb);


	/* initialize indication leds */
	BSP_LED_EnableAll();


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
	Retcode_T ble_sts;
	BaseType_t sem_sts;


	/* creates semaphore to wait stack ready */
	ble_sent_signal = xSemaphoreCreateBinary();
	assert(ble_sent_signal != NULL);

	ble_sent_sema = xSemaphoreCreateMutex();
	assert(ble_sent_sema != NULL);

	/* create queue/timers for communication channel */
	ble_rx_queue = xQueueCreate(BLE_QUEUE_SLOTS, sizeof(ble_data_t));
	assert(ble_rx_queue != NULL);

    BleStartSyncSemphr = xSemaphoreCreateBinary();
    BleWakeUpSyncSemphr = xSemaphoreCreateBinary();

    /* initialize the core stack and the BLE services */
    ble_sts = BlePeripheral_Initialize(ble_notify_cb, ble_app_service_register_cb);
    ble_sts = BlePeripheral_SetDeviceName((uint8_t *) ble_dev_name);

    ble_sts = BlePeripheral_Start();
    sem_sts = xSemaphoreTake(BleStartSyncSemphr, BLE_START_SYNC_TIMEOUT);
    assert(sem_sts == pdTRUE);

    ble_sts = BlePeripheral_Wakeup();
	sem_sts = xSemaphoreTake(BleWakeUpSyncSemphr, BLE_WAKEUP_SYNC_TIMEOUT);
	assert(sem_sts == pdTRUE);

	err = xTaskCreate(ble_app_task,"ble_app",BLE_APP_STK_SIZE,NULL,BLE_APP_PRIO,NULL);
	assert(err == pdPASS);
}
