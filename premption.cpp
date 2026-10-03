#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void low_priority_task(void *pvParameters)
{
    int counter = 0;

    while (1)
    {
        printf("LOW  | counter = %d | running...\n", counter++);

        /*
         * CPU-intensive work.
         * LOW remains Ready/Running.
         */
        for (volatile int i = 0; i < 3000000; i++);

        printf("LOW  | counter = %d | still running\n", counter++);

        for (volatile int i = 0; i < 3000000; i++);
    }
}

void high_priority_task(void *pvParameters)
{
    while (1)
    {
        /*
         * HIGH sleeps.
         * LOW gets the CPU.
         */
        vTaskDelay(pdMS_TO_TICKS(2000));

        printf("\n");
        printf("HIGH WAKE UP!\n");
        printf("HIGH LOW WAS RUNNING\n");
        printf("HIGH PREEMPTING LOW <<<\n");
        printf("\n");

        /*
         * Simulate HIGH priority work.
         */
        for (volatile int i = 0; i < 1000000; i++);

        printf("HIGH | work finished\n");
        printf("HIGH | going to sleep...\n\n");
    }
}

void app_main(void)
{
    /*
     * Both tasks are pinned to CPU 0.
     *
     * This makes the preemption easy to observe:
     */

    xTaskCreatePinnedToCore(
        low_priority_task,
        "LOW",
        4096,
        NULL,
        1,
        NULL,
        0
    );

    xTaskCreatePinnedToCore(
        high_priority_task,
        "HIGH",
        4096,
        NULL,
        5,
        NULL,
        0
    );
}