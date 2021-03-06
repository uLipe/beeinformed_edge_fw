/*
 * app_ble.h
 *
 *  Created on: 01/06/2017
 *      Author: root
 */

#ifndef APP_BLE_H_
#define APP_BLE_H_


/** maximum payload in bytes */
#define PACKET_MAX_PAYLOAD 16

/** audio time capture in ms */
#define BLE_APP_AUDIO_CAPTURE_PERIOD  11


/** packet types */
typedef enum {
	k_command_packet = 0,
	k_data_packet,
	k_sequence_packet,
}pack_type_t;

/** command list */
typedef enum {
	k_get_sensors = 1,
	k_get_audio,
	k_get_status,
	k_reboot,
}edge_cmds_t;


/** packet structure */
typedef struct {
	uint8_t type;
	uint8_t id;
	uint8_t pack_amount;
	uint8_t payload_size;
	uint8_t pack_data[PACKET_MAX_PAYLOAD];
}ble_data_t;


/*
 * @brief initializes ble channel
 */
void app_ble_init(void);


#endif /* SOURCE_APP_BLE_H_ */
