#ifndef MY_BLUETOOTH_TASK_H_
#define MY_BLUETOOTH_TASK_H_

#include "app_flash_common.h"
#include "app_bt_bonding.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "mtb_kvstore.h"
#include <FreeRTOS.h>
#include <task.h>
#include "cycfg_bt_settings.h"
#include "wiced_bt_stack.h"
#include "cybsp_bt_config.h"
#include "cybt_platform_config.h"
#include "app_bt_event_handler.h"
#include "app_bt_gatt_handler.h"
#include "app_hw_device.h"
#include "app_bt_utils.h"


void task_bluetooth_init();

#endif