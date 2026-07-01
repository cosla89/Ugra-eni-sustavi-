#pragma once

#include <stdint.h>

#define SDCARD_LOG_START_BLOCK 2048UL

typedef enum {
    SD_ERR_NONE      = 0,
    SD_ERR_CMD0      = 1,
    SD_ERR_CMD8_VOLT = 2,
    SD_ERR_ACMD41    = 3,
    SD_ERR_CMD58     = 4,
    SD_ERR_CMD16     = 5,
} sdcard_err_t;

uint8_t      sdcard_init(void);
sdcard_err_t sdcard_init_debug(void);
uint8_t      sdcard_write_block(uint32_t block, const uint8_t *data);