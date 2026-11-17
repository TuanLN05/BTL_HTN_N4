#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

// STM32F103C8T6, HSI 8 MHz, FreeRTOS tick = 1 ms.
#define configUSE_PREEMPTION                  1
#define configUSE_TIME_SLICING                1
#define configCPU_CLOCK_HZ                    ( 8000000UL )
#define configTICK_RATE_HZ                    ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES                  4
#define configMINIMAL_STACK_SIZE              128
#define configTOTAL_HEAP_SIZE                 ( 10 * 1024 )
#define configMAX_TASK_NAME_LEN               12
#define configUSE_16_BIT_TICKS                0
#define configIDLE_SHOULD_YIELD               1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1

#define configSUPPORT_STATIC_ALLOCATION       0
#define configSUPPORT_DYNAMIC_ALLOCATION      1
#define configUSE_IDLE_HOOK                   0
#define configUSE_TICK_HOOK                   0
#define configUSE_TIMERS                      0
#define configUSE_CO_ROUTINES                 0
#define configUSE_MUTEXES                     1
#define configUSE_RECURSIVE_MUTEXES           0
#define configUSE_COUNTING_SEMAPHORES         0
#define configUSE_TRACE_FACILITY              0
#define configCHECK_FOR_STACK_OVERFLOW        0
#define configUSE_MALLOC_FAILED_HOOK          0
#define configQUEUE_REGISTRY_SIZE             0

#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_vTaskDelay                    1
#define INCLUDE_vTaskDelete                   0
#define INCLUDE_vTaskPrioritySet              0
#define INCLUDE_uxTaskPriorityGet             0
#define INCLUDE_vTaskSuspend                  0

// Cortex-M3: 4 implemented priority bits.
#define configPRIO_BITS                       4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY      ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

// Use FreeRTOS implementations in the CMSIS interrupt vector.
#define vPortSVCHandler                       SVC_Handler
#define xPortPendSVHandler                    PendSV_Handler
#define xPortSysTickHandler                   SysTick_Handler

#define configASSERT(x) do { if ((x) == 0) { __asm volatile ("cpsid i"); for (;;) {} } } while (0)

#endif
