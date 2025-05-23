/******************************************************************************
* File Name:   tcp_client.h
*
* Description: This file contains declaration of task related to TCP client
* operation.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2019-2024, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#ifndef TCP_CLIENT_H_
#define TCP_CLIENT_H_

#include <cy_http_client_api.h>

/* HTTP 配置 */
#define HTTP_SERVER_HOST          "123.60.80.170"
#define HTTP_SERVER_PORT          10010
#define HTTP_API_PATH             "/api/furniture/light/status"
#define HTTP_REQUEST_TIMEOUT_MS   5000  // 5秒超时
#define HTTP_POLL_INTERVAL_MS     1000  // 1秒轮询一次
#define HTTP_RESPONSE_BUFFER_SIZE 1024   // 响应缓冲区大小

/*******************************************************************************
* Function Prototype
********************************************************************************/
void tcp_client_task(void *arg);

#endif /* TCP_CLIENT_H_ */
