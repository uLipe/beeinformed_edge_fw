/**
 * @file ble_app.c
 * @brief ble play main application file
 *
 */
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "BCDS_Basics.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "ff.h"
#include "../usr_include/XdkApplicationTemplate.h"
#include "../usr_include/beeinformed.h"
/**
 * @brief prepare whole appplication
 */
void appInitSystem(xTimerHandle xTimer)
{
    (void) (xTimer);

    /* init env sensor hw */
    Retcode_T hw_err = Environmental_init(xdkEnvironmental_BME280_Handle);
    assert(hw_err == RETCODE_SUCCESS);
    hw_err = Environmental_setPowerMode(xdkEnvironmental_BME280_Handle,
    			ENVIRONMENTAL_BME280_POWERMODE_NORMAL);
    assert(hw_err == RETCODE_SUCCESS);

    /* temperature task init */
    temp_app_init();

    /* humidity task init */
    humi_app_start();

    /* pressure task init */
    pressure_app_start();

    /* init the ble server */
    app_ble_init();
}

/** ************************************************************************* */
