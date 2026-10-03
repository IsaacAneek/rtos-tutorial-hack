Embedded Systems

# RTOS for Beginners

Why we need it, and how to use FreeRTOS on the ESP32 with **ESP-IDF** + **PlatformIO**.

platformio.ini

```
[env:esp32dev]
platform  = espressif32
board     = esp32dev
framework = espidf
monitor_speed = 115200
```

Example project: a self-balancing robot (IMU → Kalman filter → motors) plus an ML task.

What is an RTOS?

- A small OS kernel whose main job is **scheduling**: it decides which **task** (a function with its own stack and its own loop) uses the CPU, and when.
- "Real-time" does not mean fast. It means **predictable**: a result arrives *before its deadline*, every time.
- **Hard** real-time: a missed deadline is a failure (airbag, reactor). **Soft** real-time: it only degrades quality (audio glitch).
- FreeRTOS is built into ESP-IDF, so every ESP32 program already runs on it. `app_main()` is itself just a task.

1 · Why RTOS?

## One super-loop, everything in order

```
void app_main(void) {
    while (1) {
        MLtask();        // takes ~800 ms
        getMPUdata();    // must run every 5 ms
        processKalman();
        getAngle();
        actuateMotor();  // must run every 5 ms
    }
}
```

Everything runs **sequentially**. The loop has no idea what is urgent.

**Nuclear reactor version:** if `MLtask()` runs long, `getSensor()` is delayed. Readings are missed, a pressure spike goes unseen, and the control response comes too late. Catastrophic.

Do the numbers

The control loop needs `getMPUdata()` and `actuateMotor()` every **5 ms**. But one pass through the loop takes `800 ms + the rest`, so the sensor is sampled about **160× too slowly**. The Kalman filter integrates stale data, the angle estimate drifts, and the motors react to a world that no longer exists.

You could chop `MLtask()` into small slices and call them between sensor reads. That works until the next feature, and then you are writing a scheduler by hand, badly.

1 · Why RTOS?

## Why does a high-priority job wait for a low-priority one?

- A super-loop has **no priorities**. Order in the code is the only "priority".
- Nothing can **interrupt** `MLtask()`; it holds the CPU until it returns.
- Critical work waits for non-critical work.

Consequences

- Missed deadlines and stale or dropped sensor data
- Unstable control (the robot falls, the reactor trips)
- Worse as features are added: every new function delays all the others

**RTOS fix:** a scheduler with **preemption**. When a higher-priority task is ready, it immediately takes the CPU from a lower-priority one. Determinism comes from *bounded response time*, not raw speed.

Task states in FreeRTOS

- **Running**: executing on a core right now.
- **Ready**: could run, waiting for a core. The scheduler always picks the highest-priority Ready task.
- **Blocked**: waiting for time (`vTaskDelay`) or an event (mutex, queue). Uses no CPU.
- **Suspended**: parked until `vTaskResume()`.

On every tick interrupt the kernel re-checks who should run. Equal-priority tasks **time-slice** in round-robin.

**Watch out: priority inversion.** A low-priority task holds a lock the high-priority task needs, so the high one is blocked by the low one. If a medium task then preempts the low one, the high task waits even longer. Mutexes with priority inheritance (slide 8) soften this.

2 · Delays

## Blocking delay() is harmful

Goal: blink LED1 at **3 Hz** (toggle every \~167 ms) and LED2 at **5 Hz** (toggle every 100 ms).

```
while (1) {
    gpio_set_level(LED1, l1 = !l1);
    delay_ms(167);              // CPU spins, LED2 can't move
    gpio_set_level(LED2, l2 = !l2);
    delay_ms(100);              // CPU spins, LED1 can't move
}
```

Result: both LEDs toggle every 267 ms. Neither frequency is right.

A busy-wait **burns the CPU** doing nothing and blocks everything else. Fixing it by hand means juggling timestamps.

What the CPU is really doing

- `delay_ms()` is typically a loop that counts until time has passed. The core sits at **100% load** doing nothing useful, and burns power.
- A button press, sensor sample or UART byte arriving during the delay is handled late, or lost.
- The usual workaround is polling `millis()` with one timestamp per job. It works for 2 LEDs, but you are now hand-writing a scheduler.

2 · Delays

## vTaskDelay() puts the task to sleep

```
void led1_task(void *arg) {
    while (1) {
        gpio_set_level(LED1, l1 = !l1);
        vTaskDelay(pdMS_TO_TICKS(167));
    }
}
void led2_task(void *arg) {
    while (1) {
        gpio_set_level(LED2, l2 = !l2);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void app_main(void) {
    xTaskCreate(led1_task, "led1", 2048, NULL, 1, NULL);
    xTaskCreate(led2_task, "led2", 2048, NULL, 1, NULL);
}
```

The task goes into the **Blocked** state, and the scheduler runs something else (or idles to save power). Each LED has its own independent timeline.

ESP-IDF's default tick is 100 Hz (10 ms). For finer timing, set `CONFIG_FREERTOS_HZ=1000` in menuconfig. Use `vTaskDelayUntil()` for fixed-period loops.

What happens at `vTaskDelay()`

- The task asks the kernel to sleep for N ticks and moves to **Blocked**.
- The scheduler runs the next Ready task (LED2, or the idle task, which can enter low-power mode).
- When the time is up, the tick interrupt moves the task back to **Ready**, and it runs as soon as it is the highest priority.

Fixed period: `vTaskDelayUntil()`

```
TickType_t last = xTaskGetTickCount();
while (1) {
    getMPUdata(); processKalman(); actuateMotor();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(5));  // steady 5 ms period
}
```

`vTaskDelay` waits *after* the work, so the period drifts by the work time. `vTaskDelayUntil` keeps a steady rhythm, which is what control loops need.

3 · Multicore

## Two cores: SMP on the ESP32

ESP32 has a dual-core Xtensa LX6: **core 0** (PRO_CPU) and **core 1** (APP_CPU). With **SMP** (Symmetric Multiprocessing) one scheduler sees both cores and runs two tasks truly in parallel.

Let the scheduler choose a core

```
xTaskCreate(
  sensorTask,   // function
  "sensor",     // name
  4096,         // stack (bytes)
  NULL,         // arg
  5,            // priority
  NULL);        // handle
```

Pin a task to a core

```
xTaskCreatePinnedToCore(
  mlTask, "ml", 8192,
  NULL, 1, NULL,
  0);   // core 0
xTaskCreatePinnedToCore(
  controlTask, "ctrl", 4096,
  NULL, 10, NULL,
  1);   // core 1
```

Typical split: heavy `MLtask` on one core, real-time control loop on the other. Note: the Wi-Fi/BT stack runs on core 0 by default. Parallel tasks sharing data is exactly where the next problem begins.

- **Affinity:** an unpinned task may run on either core (`tskNO_AFFINITY`) and can move between them. Pin a task to keep its data warm in one core's cache and isolate it from other work.
- **Priority is global.** Each core runs the highest-priority Ready task it is allowed to run. Higher number = higher priority.
- **Stack size** is in bytes in ESP-IDF. Too small means a stack-overflow crash; check with `uxTaskGetStackHighWaterMark()`.
- Debug: `printf("core %d\n", xPortGetCoreID());` inside a task shows where it runs.

Single-core chips (ESP32-C3, ESP32-S2) run the same code; tasks just take turns instead of running together.

4 · Shared data

## Race condition: two writers, one buffer

```
char msg[32];

void taskA(void *a) { while (1) { strcpy(msg, "AAAAAAAAAAAA");  vTaskDelay(1); } }
void taskB(void *a) { while (1) { strcpy(msg, "bbbbbbbbbbbb");  vTaskDelay(1); } }
void printer(void *a) { while (1) { printf("%s\n", msg); vTaskDelay(1); } }
```

`strcpy` copies byte by byte, so it is **not atomic**. A task can be preempted (or run on the other core) mid-copy:

```
AAAAAAAAAAAA
bbbbbbbbbbbb
AAAAAAbbbbbb   ← corrupted: half A, half B
```

The result depends on timing, so it appears **randomly** and is very hard to debug.

How the interleaving happens

- Task A has copied only part of its string when it is preempted, or Task B simply runs on the other core at the same moment.
- Task B writes its own string into the same bytes.
- The buffer now holds a mix of both, and the printer reads whatever is there at that instant.

The code that touches shared data is a **critical section**. A **race condition** means the result depends on who gets there first. It can corrupt integers, structs and sensor buffers too, not just strings.

4 · Shared data

## Fix it with a mutex

Idea: wrap every access to `msg` in **Take / Give**. Only one task can hold the lock at a time, so the others wait in the Blocked state (no CPU wasted) until it is released.

```
SemaphoreHandle_t lock;

void taskA(void *a) {
    while (1) {
        xSemaphoreTake(lock, portMAX_DELAY);   // enter critical section
        strcpy(msg, "AAAAAAAAAAAA");
        xSemaphoreGive(lock);                  // leave
        vTaskDelay(1);
    }
}
// taskB and printer use the same Take / Give pattern

void app_main(void) {
    lock = xSemaphoreCreateMutex();
    // ...create tasks
}
```

- Only the task holding the mutex touches `msg`. Others **block** until it is released, so no half-written strings.
- **Mutex** = ownership of a resource. A **binary semaphore** = signalling between tasks or ISRs.
- FreeRTOS mutexes use **priority inheritance**, which limits priority inversion. Keep critical sections short.
- **Use a timeout** so a stuck holder can't freeze you: `if (xSemaphoreTake(lock, pdMS_TO_TICKS(50)) == pdTRUE) { ...; xSemaphoreGive(lock); }`
- **Deadlock:** A holds lock 1 and wants lock 2 while B holds lock 2 and wants lock 1, so both wait forever. Always take multiple locks in the same order.
- **Not from an ISR:** mutexes can't be taken in interrupts. Use a binary semaphore or queue with the `...FromISR` calls instead.

5 · Scalability

## Bare-metal state machine vs RTOS tasks

Bare metal: grows tangled

```
while (1) {
  now = millis();
  if (now - tImu  >= 5)   { readImu();  }
  if (now - tCtrl >= 5)   { control();  }
  if (now - tLed  >= 167) { blink();    }
  switch (wifiState) {
    case CONNECTING: ...; break;
    case CONNECTED:  ...; break;
    case RETRY:      ...; break;
  }
  // every new feature = more flags,
  // timers, states, and risk to old code
}
```

RTOS: one task per job

```
void imuTask(void *a) {
  while (1) { readImu();
    vTaskDelay(5); }
}
void ctrlTask(void *a) {
  while (1) { control();
    vTaskDelay(5); }
}
void wifiTask(void *a) {
  connect(); // can block freely
  while (1) { handleWifi(); }
}
// add a feature = add a task
```

**Takeaway:** each task is simple and sequential, timing is owned by the scheduler, and priorities say what matters most. Adding features no longer means rewriting the loop.

Side by side

- **Adding a feature:** bare metal edits the shared loop and its timers; RTOS adds one new task with its own delay.
- **Timing bugs:** in a loop, one slow branch delays everything. With tasks, a slow task only delays tasks of lower priority.
- **Blocking calls** (Wi-Fi connect, flash write) freeze a super-loop, but are harmless inside their own task.
- **Testing:** tasks can be developed and tested one at a time; a state machine couples all states together.

**Trade-offs:** each task costs RAM (its stack) and context-switch time, and shared data now needs locks. For a blinking LED, a loop is fine. Once you have several timing-sensitive jobs, an RTOS pays for itself.