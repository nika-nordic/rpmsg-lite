/*
 * Copyright (c) 2019, Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>

#include "rpmsg_platform.h"
#include "rpmsg_env.h"
#include <ipm.h>

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 0"
#endif


static int isr_counter = 0;
static int disable_counter = 0;
static void *platform_lock;
static struct device *tx_ipm_handle = NULL;
static struct device *rx_ipm_handle = NULL;

void platform_ipm_callback(void *context, u32_t id, volatile void *data)
{
    if (id != 0)
    {
        return;
    }
#if defined(NRF5340_XXAA_APPLICATION)   //calls callback[0] given at init
    env_isr(0);
#elif defined(NRF5340_XXAA_NETWORK) //calls callback[1] given at init
    env_isr(1);
#else
    #error "Undefined core"
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
    env_lock_mutex(platform_lock);

    RL_ASSERT(0 < isr_counter);
    isr_counter--;
    if ((!isr_counter) && (rx_ipm_handle != NULL))
    {
        ipm_set_enabled(rx_ipm_handle, 0);
    }

    /* Unregister ISR from environment layer */
    env_unregister_isr(vector_id);

    env_unlock_mutex(platform_lock);

    return 0;
}

void platform_notify(unsigned int vector_id)
{
    int status;
    env_lock_mutex(platform_lock);
    RL_ASSERT(tx_ipm_handle);
    do {
        status = ipm_send(tx_ipm_handle, 0, 0, NULL, 0);
    } while (status);
    env_unlock_mutex(platform_lock);
    return;
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
    return (0 != k_is_in_isr());
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

    if ((!disable_counter) && (rx_ipm_handle != NULL))
    {
        ipm_set_enabled(rx_ipm_handle, 1);
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
    if ((!disable_counter) && (rx_ipm_handle != NULL))
    {
        ipm_set_enabled(rx_ipm_handle, 0);
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
    /* Get IPM device handle */
#if defined(NRF5340_XXAA_APPLICATION)
    tx_ipm_handle = device_get_binding("IPM_0");
    rx_ipm_handle = device_get_binding("IPM_1");
#elif defined(NRF5340_XXAA_NETWORK)
    tx_ipm_handle = device_get_binding("IPM_1");
    rx_ipm_handle = device_get_binding("IPM_0");
#else
    #error "Undefined core."
#endif

    if(!tx_ipm_handle || !rx_ipm_handle)
    {
        return -1;
    }

    /* Register application callback with no context */
    ipm_register_callback(rx_ipm_handle, platform_ipm_callback, NULL);

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
    /* Delete lock used in multi-instanced RPMsg */
    env_delete_mutex(platform_lock);
    platform_lock = NULL;
    return 0;
}
