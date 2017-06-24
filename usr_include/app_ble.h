/*
 * app_ble.h
 *
 *  Created on: 01/06/2017
 *      Author: root
 */

#ifndef APP_BLE_H_
#define APP_BLE_H_


/** maximum payload in bytes */
#define PACKET_MAX_PAYLOAD 15


/** packet types */
typedef enum {
	k_command_packet = 0,
	k_data_packet,
}pack_type_t;

/** command list */
typedef enum {
	k_get_temp = 1,
	k_get_humi,
	k_get_press,
	k_get_audio,
	k_get_lumi,
	k_get_status,
	k_reboot,
	k_set_alarm_period,
}edge_cmds_t;



/** packet structure */
typedef struct {
	uint8_t type;
	uint8_t id;
	uint8_t pack_amount;
	uint8_t pack_nbr;
	uint8_t payload_size;
	uint8_t pack_data[PACKET_MAX_PAYLOAD];
}ble_data_t;


/*
 * @brief initializes ble channel
 */
void app_ble_init(void);


#endif /* SOURCE_APP_BLE_H_ */
