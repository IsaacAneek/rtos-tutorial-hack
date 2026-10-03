#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void hello_task(void *pvParameters)
{
    char *name = (char *)pvParameters;

    while (1)
    {
        printf("Hello %s!\n", name);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    xTaskCreate(
        hello_task,
        "hello",
        2048,
        "ESP32",
        5,
        NULL
    );
}
