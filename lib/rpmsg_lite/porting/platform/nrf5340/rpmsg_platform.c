#include <stdio.h>
#include <string.h>

#include "rpmsg_platform.h"
#include "rpmsg_env.h"

#include "nrfx_ipc.h"

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 0"
#endif

static int isr_counter = 0;
static int disable_counter = 0;
static void *platform_lock;

static void platform_ipc_callback(uint8_t event_index, void * p_context)
{
#if defined(NRF5340_XXAA_APPLICATION)
    env_isr(0);
#elif defined(NRF5340_XXAA_NETWORK)
    env_isr(1);
#else
    #error "Undefined core."
#endif
}

void platform_global_isr_disable(void)
{
    __asm volatile("cpsid i");
}

void platform_global_isr_enable(void)
{
    __asm volatile("cpsie i");
}

int platform_init_interrupt(unsigned int vector_id, void *isr_data)
{
    /* Register ISR to environment layer */
    env_register_isr(vector_id, isr_data);

    env_lock_mutex(platform_lock);

    RL_ASSERT(0 <= isr_counter);

    isr_counter++;

    env_unlock_mutex(platform_lock);

    return 0;
}

int platform_deinit_interrupt(unsigned int vector_id)
{
    /* Prepare the MU Hardware */
    env_lock_mutex(platform_lock);

    RL_ASSERT(0 < isr_counter);

    isr_counter--;

    /* Unregister ISR from environment layer */
    env_unregister_isr(vector_id);

    env_unlock_mutex(platform_lock);

    return 0;
}

void platform_notify(unsigned int vector_id)
{
    env_lock_mutex(platform_lock);

    // Application MCU transmits data on channel 0 and receives data on channel 1.
    // Network MCU transmits data on channel 1 and receives data on channel 0.
    // Both use TASKS_SEND[0] of their respective IPC peripheral.
    nrfx_ipc_signal(0);

    env_unlock_mutex(platform_lock);
}

/**
 * platform_time_delay
 *
 * @param num_msec Delay time in ms.
 *
 * This is not an accurate delay, it ensures at least num_msec passed when return.
 */
void platform_time_delay(int num_msec)
{
    while (num_msec--)
    {
        NRFX_DELAY_US(1000);
    }
}

/**
 * platform_in_isr
 *
 * Return whether CPU is processing IRQ
 *
 * @return True for IRQ, false otherwise.
 *
 */
int platform_in_isr(void)
{
    return ((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0);
}

/**
 * platform_interrupt_enable
 *
 * Enable peripheral-related interrupt
 *
 * @param vector_id Virtual vector ID that needs to be converted to IRQ number
 *
 * @return vector_id Return value is never checked.
 *
 */
int platform_interrupt_enable(unsigned int vector_id)
{
    RL_ASSERT(0 < disable_counter);

    platform_global_isr_disable();
    disable_counter--;

    if (!disable_counter)
    {
        // Application MCU transmits data on channel 0 and receives data on channel 1.
        // Network MCU transmits data on channel 1 and receives data on channel 0.
        // Both use EVENTS_RECEIVE[0] of their respective IPC peripheral.
        nrfx_ipc_receive_event_enable(0);
    }
    platform_global_isr_enable();
    return (vector_id);
}

/**
 * platform_interrupt_disable
 *
 * Disable peripheral-related interrupt.
 *
 * @param vector_id Virtual vector ID that needs to be converted to IRQ number
 *
 * @return vector_id Return value is never checked.
 *
 */
int platform_interrupt_disable(unsigned int vector_id)
{
    RL_ASSERT(0 <= disable_counter);

    platform_global_isr_disable();
    /* virtqueues use the same NVIC vector
       if counter is set - the interrupts are disabled */
    if (!disable_counter)
    {
        // Application MCU transmits data on channel 0 and receives data on channel 1.
        // Network MCU transmits data on channel 1 and receives data on channel 0.
        // Both use EVENTS_RECEIVE[0] of their respective IPC peripheral.
        nrfx_ipc_receive_event_disable(0);
    }
    disable_counter++;
    platform_global_isr_enable();
    return (vector_id);
}

/**
 * platform_map_mem_region
 *
 * Dummy implementation
 *
 */
void platform_map_mem_region(unsigned int vrt_addr, unsigned int phy_addr, unsigned int size, unsigned int flags)
{
}

/**
 * platform_cache_all_flush_invalidate
 *
 * Dummy implementation
 *
 */
void platform_cache_all_flush_invalidate(void)
{
}

/**
 * platform_cache_disable
 *
 * Dummy implementation
 *
 */
void platform_cache_disable(void)
{
}

/**
 * platform_vatopa
 *
 * Dummy implementation
 *
 */
unsigned long platform_vatopa(void *addr)
{
    return ((unsigned long)addr);
}

/**
 * platform_patova
 *
 * Dummy implementation
 *
 */
void *platform_patova(unsigned long addr)
{
    return ((void *)addr);
}

/**
 * platform_init
 *
 * platform/environment init
 */
int platform_init(void)
{
    nrfx_err_t status;
    status = nrfx_ipc_init(platform_ipc_callback, 0, NULL);
    if (status != NRFX_SUCCESS)
    {
        return status;
    }

#if defined(NRF5340_XXAA_APPLICATION)
    // Application MCU transmits data on channel 0 and receives data on channel 1
    nrfx_ipc_send_task_channel_assign(0, 0);
    nrfx_ipc_receive_event_channel_assign(0, 1);
#elif defined(NRF5340_XXAA_NETWORK)
    // Network MCU transmits data on channel 1 and receives data on channel 0
    nrfx_ipc_send_task_channel_assign(0, 1);
    nrfx_ipc_receive_event_channel_assign(0, 0);
#else
    #error "Undefined core."
#endif

    /* Create lock used in multi-instanced RPMsg */
    if(0 != env_create_mutex(&platform_lock, 1))
    {
        return -1;
    }

    return 0;
}

/**
 * platform_deinit
 *
 * platform/environment deinit process
 */
int platform_deinit(void)
{
    nrfx_ipc_uninit();

    /* Delete lock used in multi-instanced RPMsg */
    env_delete_mutex(platform_lock);
    platform_lock = NULL;
    return 0;
}
