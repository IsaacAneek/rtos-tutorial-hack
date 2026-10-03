#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUFFER_SIZE 32

char shared_buffer[BUFFER_SIZE];

void writer_a(void *pvParameters)
{
    const char *message = "AAAAAAAAAAAAAAAAAAAA";

    while (1) {

        for (int i = 0; i < strlen(message); i++) {

            /*
             * UNSAFE:
             * Task A modifies shared_buffer while
             * Task B may also be modifying it.
             */
            shared_buffer[i] = message[i];

            /*
             * Force a context switch / scheduling opportunity
             * in the middle of the write.
             */
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void writer_b(void *pvParameters)
{
    const char *message = "BBBBBBBBBBBBBBBBBBBB";

    while (1) {

        for (int i = 0; i < strlen(message); i++) {

            /*
             * SAME BUFFER!
             *
             * Task A could be writing here at the same time.
             */
            shared_buffer[i] = message[i];

            vTaskDelay(pdMS_TO_TICKS(1));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void reader_task(void *pvParameters)
{
    while (1) {

        /*
         * Task 3 reads the buffer while Tasks 1/2
         * may be modifying it.
         */
        printf("BUFFER: [%s]\n", shared_buffer);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    memset(shared_buffer, '.', BUFFER_SIZE);
    shared_buffer[BUFFER_SIZE - 1] = '\0';

    xTaskCreate(
        writer_a,
        "Writer A",
        2048,
        NULL,
        1,
        NULL
    );

    xTaskCreate(
        writer_b,
        "Writer B",
        2048,
        NULL,
        1,
        NULL
    );

    xTaskCreate(
        reader_task,
        "Reader",
        2048,
        NULL,
        2,
        NULL
    );
}