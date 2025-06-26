#include "audio_task.h"
#include "emfile_task.h"

#include "random_tool.h"

static char random_filename[13];

static void check_error(char *message, int error)
{
    if (error < 0)
    {
        printf("\n================================================================================\n");
        printf("\nFAIL: %s\n", message);
        printf("Error Value: %d\n", error);
        printf("emFile-defined Error Message: %s", FS_ErrorNo2Text(error));
        printf("\n================================================================================\n");
        
        while(true);
    }
}

void emfile_task(void* arg)
{
    int error;
    FS_FILE *file_ptr;
    const char *volume_name = "";

    // --- 这部分初始化代码只需要执行一次 ---
    printf("emFile Task Initializing...\n");
    FS_Init();

#if !defined(USE_SD_CARD)
    error = FS_FormatLLIfRequired(volume_name);
    check_error("Error in low-level formatting", error);
#endif

    error = FS_IsHLFormatted(volume_name);
    check_error("Error in checking if volume is high-level formatted", error);

    if (error == 0)
    {
        printf("Perform high-level format\n");
        error = FS_Format(volume_name, NULL);
        check_error("Error in high-level formatting", error);
    }
    printf("emFile Task ready to receive write events.\n\n");


    // --- 主循环，等待事件来触发写入 ---
    for (;;)
    {
    	uint32_t notification_value = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (notification_value > 0)
        {
            printf("Write event received! Writing to file...\n");
            generate_password();
            memset(random_filename, '\0', sizeof random_filename);
            snprintf(random_filename, sizeof random_filename, "%.8s.bin", (char*)password);
            random_filename[13] = '\0';

            // 'a' 模式表示追加 (append)。如果文件不存在则创建。
            file_ptr = FS_FOpen(random_filename, "ab");

            if(file_ptr != NULL)
            {
                U32 bytes_written = FS_Write(file_ptr, audio_frame, PDM_PCM_BUFFER_SIZE);

                if(bytes_written != PDM_PCM_BUFFER_SIZE)
                {
                    error = FS_FError(file_ptr);
                    check_error("Error in writing to the file", error);
                }

                error = FS_FClose(file_ptr);
                check_error("Error in closing the file", error);

                printf("Successfully wrote to %s\n", random_filename);
            }
            else
            {
                printf("Unable to open the file for writing!\n");
            }
        }
    }
}
