#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/* HTTP客户端包含文件 */
#include "cy_http_client_api.h"

/* WiFi连接管理器头文件 */
#include "cy_wcm.h"
#include "cy_wcm_error.h"

/*******************************************************************************
* Macros
********************************************************************************/
/* Define how many samples in a frame */
#define PDM_PCM_BUFFER_SIZE                 (224000u) // 16000 * 2 * 7 (7 seconds)

/* WiFi 连接配置 */
#define WIFI_SSID                             "spike"
#define WIFI_PASSWORD                         "lzqysq666"
#define WIFI_SECURITY_TYPE                    CY_WCM_SECURITY_WPA2_AES_PSK
#define MAX_WIFI_CONN_RETRIES                 (10u)
#define WIFI_CONN_RETRY_INTERVAL_MSEC         (1000u)
#define WIFI_INTERFACE_TYPE                   CY_WCM_INTERFACE_TYPE_STA

/* HTTP 客户端定义 */
#define SERVER_HOST                          "123.60.80.17"
#define SERVER_PORT                          11111
#define API_PATH                             "/api/audio/raw-data"
#define TRANSPORT_SEND_RECV_TIMEOUT_MS       (5000)
#define USER_BUFFER_LENGTH                   (2048)

/* Desired sample rate. Typical values: 8/16/22.05/32/44.1/48kHz */
#define SAMPLE_RATE_HZ                      16000u
/* Decimation Rate of the PDM/PCM block. Typical value is 64 */
#define DECIMATION_RATE                     64u
/* Audio Subsystem Clock. Typical values depends on the desire sample rate:
- 8/16/48kHz    : 24.576 MHz
- 22.05/44.1kHz : 22.579 MHz */
#define AUDIO_SYS_CLOCK_HZ                  24576000u
/* PDM/PCM Pins */
#define PDM_DATA                            P10_5
#define PDM_CLK                             P10_4
/* Switch press/release check interval in milliseconds for debouncing */
#define SWITCH_DEBOUNCE_CHECK_UNIT          (1u)
/* Number of debounce check units to count before considering that switch is pressed or released */
#define SWITCH_DEBOUNCE_MAX_PERIOD_UNITS    (80u)

#define AMBIENT_TEMPERATURE_C               (20)
#define SPI_BAUD_RATE_HZ                    (20000000)


/*******************************************************************************
* Function Prototypes
********************************************************************************/
void clock_init(void);
void pdm_pcm_isr_handler(void *arg, cyhal_pdm_pcm_event_t event);
static uint32_t read_switch_status(void);
cy_rslt_t send_audio_data_to_server(uint8_t *audio_data, uint32_t data_size);
static cy_rslt_t connect_to_wifi_ap(void);

/*******************************************************************************
* Global Variables
********************************************************************************/
volatile bool pdm_pcm_flag = false;

/* HTTP 客户端变量 */
cy_http_client_t http_client_handle;
cy_awsport_server_info_t server_info;
uint8_t http_buffer[USER_BUFFER_LENGTH];

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



/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for CM4 CPU. It sets up a timer to trigger a
* periodic interrupt. The main while loop checks for the status of a flag set
* by the interrupt and toggles an LED at 1Hz to create an LED blinky. The
* while loop also checks whether the 'Enter' key was pressed and
* stops/restarts LED blinking.
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    uint8_t  audio_frame[PDM_PCM_BUFFER_SIZE] = {0};

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Init the clocks */
    clock_init();

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);


    /* Initialize the User LEDs */
    cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    /* Initialize the User button */
    result = cyhal_gpio_init(CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, 1);

    /* GPIO initialization failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
    /* Initialize the PDM/PCM block */
    cyhal_pdm_pcm_init(&pdm_pcm, PDM_DATA, PDM_CLK, &audio_clock, &pdm_pcm_cfg);
    cyhal_pdm_pcm_register_callback(&pdm_pcm, pdm_pcm_isr_handler, NULL);
    cyhal_pdm_pcm_enable_event(&pdm_pcm, CYHAL_PDM_PCM_ASYNC_COMPLETE, CYHAL_ISR_PRIORITY_DEFAULT, true);
    cyhal_pdm_pcm_start(&pdm_pcm);

    /* To avoid compiler warning */
    (void)result;
    
    /* 初始化 WiFi 连接管理器 */
    cy_wcm_config_t wifi_config = { .interface = WIFI_INTERFACE_TYPE };
    
    result = cy_wcm_init(&wifi_config);

    if (result != CY_RSLT_SUCCESS)
    {
        printf("Wi-Fi Connection Manager initialization failed! Error code: 0x%08lx\r\n", (uint32_t)result);
        CY_ASSERT(0);
    }
    printf("Wi-Fi Connection Manager initialized.\r\n");
    
    /* 连接到 WiFi AP */
    result = connect_to_wifi_ap();
    if(result != CY_RSLT_SUCCESS)
    {
//        printf("\n 无法连接到WiFi网络！错误码: 0x%08lx\r\n", (uint32_t)result);
        CY_ASSERT(0);
    }

    /* 初始化 HTTP 客户端库 */
    result = cy_http_client_init();
    if (result != CY_RSLT_SUCCESS)
    {
//        printf("HTTP客户端初始化失败\r\n");
        CY_ASSERT(0);
    }
    
    /* 配置服务器信息 */
    server_info.host_name = SERVER_HOST;
    server_info.port = SERVER_PORT;

    /* 创建 HTTP 客户端实例 */
    result = cy_http_client_create(NULL, &server_info, NULL, NULL, &http_client_handle);
    if (result != CY_RSLT_SUCCESS)
    {
//        printf("HTTP客户端创建失败\r\n");
        CY_ASSERT(0);
    }

    /* Show the startup screen */
//    printf("系统已初始化，按下按钮开始录音\r\n");

    for(;;)
    {
        /* Check if any microphone has data to process */
        if (pdm_pcm_flag)
        {
        	//printf("\nWriting to UART...\n");
            cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_ON);

            /* Clear the PDM/PCM flag */
            pdm_pcm_flag = 0;


            printf("录音完成，正在发送数据到服务器...\r\n");
            
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
            
            cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_OFF);
            printf("按下按钮开始新的录音\r\n");

        }
        if (0UL != read_switch_status())
        {
            //printf("Recording\n");

            cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_ON);
            cyhal_system_delay_ms(1000);

            /* Read the next frame */
            cyhal_pdm_pcm_read_async(&pdm_pcm, audio_frame, PDM_PCM_BUFFER_SIZE/2);
            cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_OFF);
        }

        cyhal_system_delay_ms(1);
    }
}


void pdm_pcm_isr_handler(void *arg, cyhal_pdm_pcm_event_t event)
{
    (void) arg;
    (void) event;

    pdm_pcm_flag = true;
}

void clock_init(void)
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

uint32_t read_switch_status(void)
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
* Function Name: connect_to_wifi_ap
********************************************************************************
* Summary:
* 连接到WiFi AP，使用用户配置的凭证，重试直到配置的重试次数或者连接成功
*
* Return:
*  cy_rslt_t - 操作结果
*
*******************************************************************************/
static cy_rslt_t connect_to_wifi_ap(void)
{
    cy_rslt_t result;
    cy_wcm_connect_params_t wifi_conn_param;
    cy_wcm_ip_address_t ip_address;

    /* 设置Wi-Fi SSID、密码和安全类型 */
    memset(&wifi_conn_param, 0, sizeof(cy_wcm_connect_params_t));
    memcpy(wifi_conn_param.ap_credentials.SSID, WIFI_SSID, sizeof(WIFI_SSID));
    memcpy(wifi_conn_param.ap_credentials.password, WIFI_PASSWORD, sizeof(WIFI_PASSWORD));
    wifi_conn_param.ap_credentials.security = WIFI_SECURITY_TYPE;

    printf("正在连接到WiFi网络: %s\r\n", WIFI_SSID);

    /* 加入WiFi AP */
    for(uint32_t conn_retries = 0; conn_retries < MAX_WIFI_CONN_RETRIES; conn_retries++)
    {
        result = cy_wcm_connect_ap(&wifi_conn_param, &ip_address);

        if(result == CY_RSLT_SUCCESS)
        {
            printf("成功连接到WiFi网络 '%s'\r\n", wifi_conn_param.ap_credentials.SSID);
            printf("IP 地址: %d.%d.%d.%d\r\n", 
                   (uint8_t)(ip_address.ip.v4),
                   (uint8_t)(ip_address.ip.v4 >> 8),
                   (uint8_t)(ip_address.ip.v4 >> 16),
                   (uint8_t)(ip_address.ip.v4 >> 24));
            return result;
        }

        printf("连接到WiFi网络失败，错误码: %d\r\n", (int)result);
        printf("%d 毫秒后重试...\r\n", WIFI_CONN_RETRY_INTERVAL_MSEC);

        cyhal_system_delay_ms(WIFI_CONN_RETRY_INTERVAL_MSEC);
    }

    /* 超过最大重试次数后停止重试 */
    printf("超过最大WiFi连接尝试次数\r\n");

    return result;
}

/*******************************************************************************
* Function Name: send_audio_data_to_server
********************************************************************************
* Summary:
* 将录制的音频数据通过HTTP POST请求发送到服务器
*
* Parameters:
*  audio_data - 指向音频数据的指针
*  data_size - 音频数据的大小（字节）
*
* Return:
*  cy_rslt_t - 操作结果
*
*******************************************************************************/
cy_rslt_t send_audio_data_to_server(uint8_t *audio_data, uint32_t data_size)
{
    cy_rslt_t result;
    cy_http_client_request_header_t request;
    cy_http_client_header_t headers[2];
    uint32_t num_headers = 2;
    cy_http_client_response_t response;
    
    /* 连接到HTTP服务器 */
    result = cy_http_client_connect(http_client_handle, TRANSPORT_SEND_RECV_TIMEOUT_MS, TRANSPORT_SEND_RECV_TIMEOUT_MS);
    if (result != CY_RSLT_SUCCESS)
    {
        printf("无法连接到服务器\r\n");
        return result;
    }
    
    /* 配置请求头 */
    request.buffer = http_buffer;
    request.buffer_len = USER_BUFFER_LENGTH;
    request.headers_len = 0;
    request.method = CY_HTTP_CLIENT_METHOD_POST;
    request.range_end = -1;
    request.range_start = 0;
    request.resource_path = API_PATH;
    
    /* 设置自定义请求头 */
    headers[0].field = "Content-Type";
    headers[0].field_len = strlen("Content-Type");
    headers[0].value = "application/octet-stream";
    headers[0].value_len = strlen("application/octet-stream");
    
    headers[1].field = "Content-Length";
    headers[1].field_len = strlen("Content-Length");
    char content_length_str[16];
    snprintf(content_length_str, sizeof(content_length_str), "%lu", data_size);
    headers[1].value = content_length_str;
    headers[1].value_len = strlen(content_length_str);
    
    /* 生成HTTP请求头 */
    result = cy_http_client_write_header(http_client_handle, &request, headers, num_headers);
    if (result != CY_RSLT_SUCCESS)
    {
        printf("写入HTTP头失败\r\n");
        cy_http_client_disconnect(http_client_handle);
        return result;
    }
    
    /* 发送HTTP请求和音频数据到服务器 */
    result = cy_http_client_send(http_client_handle, &request, audio_data, data_size, &response);
    if (result != CY_RSLT_SUCCESS)
    {
        printf("发送HTTP请求失败\r\n");
        cy_http_client_disconnect(http_client_handle);
        return result;
    }
    
    /* 检查HTTP响应码 */
    printf("服务器响应码: %ld\r\n", response.status_code);
    
    /* 断开与服务器的连接 */
    result = cy_http_client_disconnect(http_client_handle);
    if (result != CY_RSLT_SUCCESS)
    {
        printf("断开与服务器的连接失败\r\n");
        return result;
    }
    
    return CY_RSLT_SUCCESS;
}

