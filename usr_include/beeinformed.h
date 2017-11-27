/*
 * beeinformed.h
 *
 *  Created on: 08/06/2017
 *      Author: root
 */

#ifndef USR_INCLUDE_BEEINFORMED_H_
#define USR_INCLUDE_BEEINFORMED_H_


#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "BCDS_Basics.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"
#include "task.h"
#include "timers.h"

#include "BCDS_Retcode.h"
#include "BSP_BoardType.h"
#include "bsp/BCDS_BSP_Board.h"
#include "bsp/BCDS_BSP_LED.h"
#include "bsp/BCDS_BSP_Mic_AKU340.h"
#include "bsp/BCDS_BSP_BT_EM9301.h"


#include "em_cmu.h"
#include "em_device.h"
#include "em_adc.h"
#include "em_dma.h"
#include "em_prs.h"
#include "em_timer.h"
#include "em_gpio.h"


#include "BCDS_BluetoothConfig.h"
#include "BCDS_BidirectionalService.h"
#include "BCDS_Ble.h"
#include "BCDS_BlePeripheral.h"
#include "BCDS_Ble_Version.h"
#include "XdkSensorHandle.h"
#include "BCDS_MCU_GPIO.h"
#include "BCDS_SensorServices.h"


#include "edge_audio_processing.h"
#include "edge_audio_acq.h"
#include "edge_sensors.h"
#include "app_ble.h"


#define TASK_PRIO_MAIN_CMD_PROCESSOR                (UINT32_C(1))
#define TASK_STACK_SIZE_MAIN_CMD_PROCESSOR          (UINT16_C(700))
#define TASK_Q_LEN_MAIN_CMD_PROCESSOR                (UINT32_C(10))


void appInitSystem(void * CmdProcessorHandle, uint32_t param2);

/**
 * func()
 * @brief
 * @param
 * @reuturn
 */

#endif /* USR_INCLUDE_BEEINFORMED_H_ */
