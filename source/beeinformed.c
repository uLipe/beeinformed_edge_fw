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

void appInitSystem(void * CmdProcessorHandle, uint32_t param2)
{
    if (!CmdProcessorHandle)
    {
        printf("Command processor handle is null \n\r");
        assert(false);
    }
    /* init the ble server */
    app_ble_init();

    BCDS_UNUSED(param2);
}


/** ************************************************************************* */
