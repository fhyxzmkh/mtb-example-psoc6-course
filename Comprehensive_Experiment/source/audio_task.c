/******************************************************************************
* File Name:   audio_task.c
*
* Description: This file contains task and functions related to audio recording
* operation.
*
*******************************************************************************/

/* Header file includes. */
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/* RTOS header file. */
#include "cyabs_rtos.h"

/* Standard C header file. */
#include <string.h>
#include <stdlib.h>

/* TCP client task header file. */
#include "udp_client.h"
#include "audio_task.h"
#include "led_task.h"

/*******************************************************************************
* Function Prototypes
********************************************************************************/
static void pdm_pcm_isr_handler(void *arg, cyhal_pdm_pcm_event_t event);
static uint32_t read_switch_status(void);
static void clock_init(void);

/*******************************************************************************
* Global Variables
********************************************************************************/
volatile bool pdm_pcm_flag = false;

/* HAL Object */
cyhal_pdm_pcm_t pdm_pcm;
cyhal_clock_t   audio_clock;
cyhal_clock_t   pll_clock;

/* HAL Config */
const cyhal_pdm_pcm_cfg_t pdm_pcm_cfg =
{
    .sample_rate     = SAMPLE_RATE_HZ,
    .decimation_rate = DECIMATION_RATE,
    .mode            = CYHAL_PDM_PCM_MODE_LEFT,
    .word_length     = 16,  /* bits */
    .left_gain       = CY_PDM_PCM_GAIN_10_5_DB,   /* dB */
    .right_gain      = 0U,   /* dB */
};

/* Pointer to the new frame that need to be written */
uint8_t *current_frame;

uint8_t audio_frame[PDM_PCM_BUFFER_SIZE] = {0};

/*******************************************************************************
* Function Name: audio_task
********************************************************************************
* Summary:
*  Task function that handles audio recording and sending to TCP server.
*
* Parameters:
*  void *arg : Task parameter defined during task creation (unused).
*
* Return:
*  void
*
*******************************************************************************/
void audio_task(void *arg)
{
    cy_rslt_t result;
    
    /* To avoid compiler warning */
    (void)arg;

    /* 初始化音频系统 */
    result = init_audio_system();
    if (result != CY_RSLT_SUCCESS)
    {
        printf("音频系统初始化失败，错误码: %ld\r\n", result);
        return;
    }

    /* 初始化WIFI */
    result = init_wifi();
    if (result != CY_RSLT_SUCCESS)
    {
        printf("WIFI初始化失败，错误码: %ld\r\n", result);
        return;
    }

    printf("音频任务已启动，按下按钮开始录音\r\n");
    init_ok = true;

    //led_command_data_t led_cmd_data;

    for(;;)
    {
        /* Check if any microphone has data to process */
        if (pdm_pcm_flag)
        {
            // cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_ON);


            /* Clear the PDM/PCM flag */
            pdm_pcm_flag = 0;

            if (emfile_task_handle != NULL)
            {
                printf("录音完成，正在保存数据到SD卡...\r\n");
                xTaskNotifyGive(emfile_task_handle);
            }

            printf("录音完成，正在发送数据到UDP服务器...\r\n");

            /* 发送音频数据到服务器 */
            result = send_audio_data_to_server(audio_frame, PDM_PCM_BUFFER_SIZE);

            if (result != CY_RSLT_SUCCESS)
            {
                printf("发送数据到服务器失败，错误码: %ld\r\n", result);
            }
            else
            {
                printf("数据发送成功！\r\n");
            }
            
            // cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_OFF);
            
            printf("按下按钮开始新的录音\r\n");
        }
        
        if (0UL != read_switch_status())
        {
            printf("开始录音...\r\n");

            // cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_ON);
            // led_cmd_data.command = LED_TURN_ON;
            // xQueueSendToBack(led_command_data_q, &led_cmd_data, 0u);
            
            memset(audio_frame, 0, sizeof(audio_frame));
            cyhal_system_delay_ms(1000);

            /* Read the next frame */
            cyhal_pdm_pcm_read_async(&pdm_pcm, audio_frame, PDM_PCM_BUFFER_SIZE/2);
            // cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_OFF);
            // led_cmd_data.command = LED_TURN_OFF;
            // xQueueSendToBack(led_command_data_q, &led_cmd_data, 0u);
        }

        cyhal_system_delay_ms(1);
    }
}

/*******************************************************************************
* Function Name: init_audio_system
********************************************************************************
* Summary:
*  Initializes the audio system including PDM/PCM block.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
cy_rslt_t init_audio_system(void)
{
    cy_rslt_t result;
    
    /* Init the clocks */
    clock_init();

    /* Initialize the User button */
    result = cyhal_gpio_init(CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, 1);

    /* GPIO initialization failed. Return error */
    if (result != CY_RSLT_SUCCESS)
    {
        printf("按钮初始化失败，错误码: %ld\r\n", result);
        return result;
    }
    
    /* Initialize the PDM/PCM block */
    result = cyhal_pdm_pcm_init(&pdm_pcm, PDM_DATA, PDM_CLK, &audio_clock, &pdm_pcm_cfg);
    if (result != CY_RSLT_SUCCESS)
    {
        printf("PDM/PCM初始化失败，错误码: %ld\r\n", result);
        return result;
    }
    
    cyhal_pdm_pcm_register_callback(&pdm_pcm, pdm_pcm_isr_handler, NULL);
    cyhal_pdm_pcm_enable_event(&pdm_pcm, CYHAL_PDM_PCM_ASYNC_COMPLETE, CYHAL_ISR_PRIORITY_DEFAULT, true);
    
    result = cyhal_pdm_pcm_start(&pdm_pcm);
    if (result != CY_RSLT_SUCCESS)
    {
        printf("PDM/PCM启动失败，错误码: %ld\r\n", result);
        return result;
    }
    
    printf("音频系统初始化完成\r\n");
    return CY_RSLT_SUCCESS;
}

/*******************************************************************************
* Function Name: pdm_pcm_isr_handler
********************************************************************************
* Summary:
*  PDM/PCM interrupt handler.
*
* Parameters:
*  void *arg: Pointer to the argument passed during initialization
*  cyhal_pdm_pcm_event_t event: PDM/PCM event type
*
* Return:
*  None
*
*******************************************************************************/
static void pdm_pcm_isr_handler(void *arg, cyhal_pdm_pcm_event_t event)
{
    (void) arg;
    (void) event;

    pdm_pcm_flag = true;
}

/*******************************************************************************
* Function Name: clock_init
********************************************************************************
* Summary:
*  Initializes the clocks for the audio system.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
static void clock_init(void)
{
    /* 初始化 PLL */
    cyhal_clock_reserve(&pll_clock, &CYHAL_CLOCK_PLL[0]);
    cyhal_clock_set_frequency(&pll_clock, AUDIO_SYS_CLOCK_HZ, NULL);
    cyhal_clock_set_enabled(&pll_clock, true, true);

    /* 初始化音频子系统时钟 (CLK_HF[1])
     * CLK_HF[1] 是 I2S 和 PDM/PCM 模块的根时钟 */
    cyhal_clock_reserve(&audio_clock, &CYHAL_CLOCK_HF[1]);

    /* 将音频子系统时钟源设置为 PLL */
    cyhal_clock_set_source(&audio_clock, &pll_clock);
    cyhal_clock_set_enabled(&audio_clock, true, true);
}

/*******************************************************************************
* Function Name: read_switch_status
********************************************************************************
* Summary:
*  Reads the status of the user button with debouncing.
*
* Parameters:
*  None
*
* Return:
*  uint32_t: 1 if button is pressed and released, 0 otherwise
*
*******************************************************************************/
static uint32_t read_switch_status(void)
{
    uint32_t delayCounter = 0;
    uint32_t sw_status = 0;

    /* Check if the switch is pressed */
    while(0UL == cyhal_gpio_read(CYBSP_USER_BTN))
    {
        /* Switch is pressed. Proceed for debouncing. */
        cyhal_system_delay_ms(SWITCH_DEBOUNCE_CHECK_UNIT);
        ++delayCounter;

        /* Keep checking the switch status till the switch is pressed for a
         * minimum period of SWITCH_DEBOUNCE_CHECK_UNIT x SWITCH_DEBOUNCE_MAX_PERIOD_UNITS */
        if (delayCounter > SWITCH_DEBOUNCE_MAX_PERIOD_UNITS)
        {
            /* Wait till the switch is released */
            while(0UL == cyhal_gpio_read(CYBSP_USER_BTN))
            {
            }

            /* Debounce when the switch is being released */
            do
            {
                delayCounter = 0;

                while(delayCounter < SWITCH_DEBOUNCE_MAX_PERIOD_UNITS)
                {
                    cyhal_system_delay_ms(SWITCH_DEBOUNCE_CHECK_UNIT);
                    ++delayCounter;
                }

            }while(0UL == cyhal_gpio_read(CYBSP_USER_BTN));

            /* Switch is pressed and released*/
            sw_status = 1u;
        }
    }

    return (sw_status);
}

/*******************************************************************************
* Function Name: send_audio_data_to_server
********************************************************************************
* Summary:
*  Sends audio data to the UDP server.
*
* Parameters:
*  audio_data: Pointer to the audio data buffer
*  data_size: Size of the audio data in bytes
*
* Return:
*  cy_rslt_t: CY_RSLT_SUCCESS if the operation is successful, an error code otherwise
*
*******************************************************************************/
cy_rslt_t send_audio_data_to_server(uint8_t *audio_data, uint32_t data_size)
{
    cy_rslt_t result;
    
    /* 设置目标服务器地址和端口 */
    cy_socket_sockaddr_t server_addr = {
        .ip_address.ip.v4 = UDP_SERVER_IP_ADDRESS,
        .ip_address.version = CY_SOCKET_IP_VER_V4,
        .port = UDP_SERVER_PORT
    };
    
    /* 检查客户端套接字是否已创建 */
    if (client_handle == NULL)
    {
        printf("错误：UDP客户端套接字未初始化\r\n");
        return CY_RSLT_MODULE_SECURE_SOCKETS_NOT_INITIALIZED;
    }

    uint32_t bytes_sent = 0;
    char* start_flag = "aaaa";
    result = cy_socket_sendto(client_handle, start_flag, sizeof(start_flag), 
                             CY_SOCKET_FLAGS_NONE, &server_addr,
                             sizeof(cy_socket_sockaddr_t), &bytes_sent);
    
    /* 发送音频数据到服务器 */
    printf("正在发送 %lu 字节的音频数据到服务器 %d.%d.%d.%d:%d...\r\n", 
           data_size,
           (uint8)(server_addr.ip_address.ip.v4),
           (uint8)(server_addr.ip_address.ip.v4 >> 8),
           (uint8)(server_addr.ip_address.ip.v4 >> 16),
           (uint8)(server_addr.ip_address.ip.v4 >> 24),
           server_addr.port);
    

    /* 分块发送音频数据，每次发送1KB */
    uint32_t total_bytes_sent = 0;
    uint32_t chunk_size = 1024;
    uint32_t remaining_bytes = data_size;

    while (remaining_bytes > 0)
    {
        uint32_t current_chunk_size = (remaining_bytes < chunk_size) ? remaining_bytes : chunk_size;
        
        /* 使用UDP发送数据 */
        result = cy_socket_sendto(client_handle, audio_data + total_bytes_sent, current_chunk_size, 
                             CY_SOCKET_FLAGS_NONE, &server_addr,
                             sizeof(cy_socket_sockaddr_t), &bytes_sent);
        
        if (result != CY_RSLT_SUCCESS)
        {
            printf("发送音频数据失败，已发送 %lu 字节\r\n", total_bytes_sent);
            return result;
        }
        
        total_bytes_sent += bytes_sent;
        remaining_bytes -= bytes_sent;
        
        /* 打印进度 */
        // if (total_bytes_sent % (chunk_size * 10) == 0 || total_bytes_sent == data_size)
        // {
        //     printf("已发送 %lu / %lu 字节 (%.1f%%)\r\n", 
        //            total_bytes_sent, data_size, 
        //            ((float)total_bytes_sent / data_size) * 100);
        // }
    }

    /* 发送结束标志位，以便server处理 */
    char* end_flag = "flag";
    result = cy_socket_sendto(client_handle, end_flag, sizeof(end_flag), 
                             CY_SOCKET_FLAGS_NONE, &server_addr,
                             sizeof(cy_socket_sockaddr_t), &bytes_sent);
    
    return CY_RSLT_SUCCESS;
}
