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
#include "../usr_include/beeinformed.h"

/**
 * @brief prepare whole appplication
 */
void appInitSystem(xTimerHandle xTimer)
{
    (void) (xTimer);

    /* init the ble server */
    app_ble_init();
}

/** ************************************************************************* */
