/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for UDP Client Example in ModusToolbox.
*              The example establishes a connection with a remote UDP server
*              and based on the command received from the UDP server, turns
*              the user LED ON or OFF.
*
* Related Document: See README.md
*******************************************************************************/

/* Header file includes. */
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "mtb_kvstore.h"

/* FreeRTOS header file. */
#include <FreeRTOS.h>
#include <task.h>

/* UDP client task header file. */
#include "udp_client.h"

#include "audio_task.h"
#include "capsense_task.h"
#include "led_task.h"

#include "emfile_task.h"

#include "bluetooth_task.h"


/*******************************************************************************
* Macros
********************************************************************************/
/* RTOS related macros. */
#define UDP_CLIENT_TASK_STACK_SIZE        (3 * 1024)
#define TASK_CAPSENSE_PRIORITY            (configMAX_PRIORITIES - 1)
#define TASK_LED_PRIORITY                 (configMAX_PRIORITIES - 2)

#define AUDIO_TASK_PRIORITY               (CY_RTOS_PRIORITY_NORMAL)
#define AUDIO_TASK_STACK_SIZE             (6 * 1024)

#define TASK_EMFILE_PRIORITY            (configMAX_PRIORITIES - 5)

#define TASK_BT_INIT_PRIORITY            (configMAX_PRIORITIES - 3)

/*******************************************************************************
* Global Variables
********************************************************************************/

/* Stack sizes of user tasks in this project */
#define TASK_CAPSENSE_STACK_SIZE (256u)
#define TASK_LED_STACK_SIZE (configMINIMAL_STACK_SIZE)

/* Queue lengths of message queues used in this project */
#define SINGLE_ELEMENT_QUEUE (1u)

/* UDP server task handle. */
TaskHandle_t client_task_handle;

TaskHandle_t audio_task_handle;

TaskHandle_t emfile_task_handle;

cyhal_trng_t trng_obj;

bool init_ok = false;

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*  System entrance point. This function sets up user tasks and then starts
*  the RTOS scheduler.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    /* \x1b[2J\x1b[;H - ANSI ESC sequence to clear screen. */
    printf("\x1b[2J\x1b[;H");
    printf("============================================================\n");
    printf("Embedded System Software Development Technology Course\n");
    printf("============================================================\n\n");


    /* Init QSPI and enable XIP to get the Wi-Fi firmware from the QSPI NOR flash */
    #if defined(CY_DEVICE_PSOC6A512K)
    const uint32_t bus_frequency = 50000000lu;
    cy_serial_flash_qspi_init(smifMemConfigs[0], CYBSP_QSPI_D0, CYBSP_QSPI_D1,
                                  CYBSP_QSPI_D2, CYBSP_QSPI_D3, NC, NC, NC, NC,
                                  CYBSP_QSPI_SCK, CYBSP_QSPI_SS, bus_frequency);

    cy_serial_flash_qspi_enable_xip(true);
    #endif

    /* Create the queues. See the respective data-types for details of queue
     * contents
     */
    led_command_data_q = xQueueCreate(SINGLE_ELEMENT_QUEUE,
                                      sizeof(led_command_data_t));

    capsense_command_q = xQueueCreate(SINGLE_ELEMENT_QUEUE,
                                      sizeof(capsense_command_t));
    

    xTaskCreate(audio_task, "Audio task", AUDIO_TASK_STACK_SIZE,
    			NULL, AUDIO_TASK_PRIORITY, &audio_task_handle);

    xTaskCreate(task_capsense, "CapSense Task", TASK_CAPSENSE_STACK_SIZE,
                NULL, TASK_CAPSENSE_PRIORITY, NULL);

    xTaskCreate(task_led, "Led Task", TASK_LED_STACK_SIZE,
                NULL, TASK_LED_PRIORITY, NULL);

    xTaskCreate(emfile_task, "Emfile Task", 512,
    			NULL, TASK_EMFILE_PRIORITY, &emfile_task_handle);

    xTaskCreate(task_bluetooth_init, "Bt Init Task", 512,
    			NULL, TASK_BT_INIT_PRIORITY, NULL);

    /* Start the FreeRTOS scheduler. */
    vTaskStartScheduler();

    /* Should never get here. */
    CY_ASSERT(0);
}

/* [] END OF FILE */
