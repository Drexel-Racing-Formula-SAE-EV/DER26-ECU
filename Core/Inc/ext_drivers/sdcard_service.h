#ifndef SDCARD_SERVICE_H
#define SDCARD_SERVICE_H
#include <stdbool.h>
#include <stdint.h>
#include "ff.h"
#include "diskio.h"
#include "user_diskio_spi.h"

typedef struct {
    bool linked, initialized, mounted;
    DSTATUS disk_status;
    FRESULT last_fresult;
    FRESULT last_error;
    user_spi_sd_diag_t protocol;
    uint32_t sector_count;
    uint16_t sector_size;
    uint32_t block_size;
    uint32_t init_attempts, mount_attempts, write_pass, read_pass, failures;
} sdcard_diag_t;

void sdcard_service_init(void);
bool sdcard_service_lock(uint32_t timeout_ms);
void sdcard_service_unlock(void);
void sdcard_service_get(sdcard_diag_t *out);
FRESULT sdcard_service_card_init(void);
FRESULT sdcard_service_mount(void);
FRESULT sdcard_service_unmount(void);
FRESULT sdcard_service_write_read_test(const char *name, uint32_t sequence);
FRESULT sdcard_service_soak(uint32_t count, uint32_t *passed);
const char *sdcard_fresult_name(FRESULT r);
#endif
