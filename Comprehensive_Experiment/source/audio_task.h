/******************************************************************************
* File Name:   audio_task.h
*
* Description: This file contains task and functions related to audio recording
* operation.
*
*******************************************************************************/

#ifndef AUDIO_TASK_H_
#define AUDIO_TASK_H_

/* RTOS header file. */
#include "cyabs_rtos.h"

#include "emfile_task.h"
#include "queue.h"

/*******************************************************************************
* Macros
********************************************************************************/
/* Define how many samples in a frame */
#define PDM_PCM_BUFFER_SIZE                 (224000u) // 16000 * 2 * 7 (7 seconds)

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

// /* Audio buffer */
// extern uint8_t* audio_frame;

/* Audio buffer */
extern uint8_t audio_frame[PDM_PCM_BUFFER_SIZE];

extern bool init_ok;

/*******************************************************************************
* Function Prototypes
********************************************************************************/
void audio_task(void *arg);
cy_rslt_t send_audio_data_to_server(uint8_t *audio_data, uint32_t data_size);
cy_rslt_t init_audio_system(void);

#endif /* AUDIO_TASK_H_ */
