#include "azjacdac.h"
#include <stdlib.h>

int jd_pin_num(void) {
    return PIN_JACDAC;
}

uint64_t hw_device_id(void) {
    static uint64_t addr;
    if (!addr) {
        uint8_t mac[6];
        esp_efuse_mac_get_default(mac);
        addr = ((uint64_t)0xff << 56) | ((uint64_t)mac[5] << 48) | ((uint64_t)mac[4] << 40) |
               ((uint64_t)mac[3] << 32) | ((uint64_t)mac[2] << 24) | ((uint64_t)mac[1] << 16) |
               ((uint64_t)mac[0] << 8) | ((uint64_t)0xfe << 0);
    }
    return addr;
}

void jd_alloc_stack_check(void) {}

void jd_alloc_init(void) {}

void *jd_alloc(uint32_t size) {
    return calloc(size, 1);
}

void jd_free(void *ptr) {
    free(ptr);
}

void *jd_alloc_emergency_area(uint32_t size) {
    return calloc(size, 1);
}

void target_reset() {
    NVIC_SystemReset();
}

void target_wait_us(uint32_t us) {
    int64_t later = esp_timer_get_time() + us;
    while (esp_timer_get_time() < later) {
        ;
    }
}

static int8_t irq_disabled;

void target_enable_irq(void) {
    irq_disabled--;
    if (irq_disabled <= 0) {
        irq_disabled = 0;
        asm volatile("cpsie i" : : : "memory");
    }
}

void target_disable_irq(void) {
    asm volatile("cpsid i" : : : "memory");
    irq_disabled++;
}

int target_in_irq(void) {
    return (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0;
}

void hw_panic(void) {
    DMESG("HW PANIC!");
    abort();
}

void reboot_to_uf2(void) {
    target_reset();
}
