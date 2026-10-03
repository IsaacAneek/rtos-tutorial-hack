#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdio.h"

void task1(void *pvParameters)
{
    while (1) {
        printf("Task 1\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void task2(void *pvParameters)
{
    while (1) {
        printf("Task 2\n");
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

void app_main(void)
{
    xTaskCreate(
        task1,
        "Task 1",
        2048,
        NULL,
        1,
        NULL
    );

    xTaskCreate(
        task2,
        "Task 2",
        2048,
        NULL,
        1,
        NULL
    );
}