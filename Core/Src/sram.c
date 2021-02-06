#include "main.h"
#include "sram.h"

#define LOG_TAG              "sram"
#define LOG_LVL              LOG_LVL_DBG
#include "ulog.h"

void sram_test(void)
{
    u32_t *sram = (u32_t *)SRAM_BASE_ADDR;

    for(u32_t i = 0; i < 0x20000; i++) {
        sram[i] = i;
    }

    for (u32_t i = 0; i < 0x20000; i++) {
        if (sram[i] != i) {
            goto fault;
        }
    }

    LOG_I("SRAM TEST OK!");
    return;

fault:
    LOG_E("SRAM test failed!");
}
