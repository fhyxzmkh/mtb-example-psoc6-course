#ifndef SOURCE_EMFILE_TASK_H_
#define SOURCE_EMFILE_TASK_H_

#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "FS.h"
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <inttypes.h>
#include <stdio.h>


#define NUM_BYTES_TO_READ_FROM_FILE         (256U)
#define DEBOUNCE_DELAY_MS                   (50U)


extern cyhal_trng_t trng_obj;

extern TaskHandle_t emfile_task_handle;


void emfile_task(void* arg);


#endif
