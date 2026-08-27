#ifndef ECU_HOST_FAKE_STORAGE_H
#define ECU_HOST_FAKE_STORAGE_H
#include "ff.h"
#include "diskio.h"
#include "user_diskio_spi.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t link_result;
    DSTATUS initialize_status;
    DSTATUS status;
    DRESULT sector_count_result;
    DRESULT sector_size_result;
    DRESULT block_size_result;
    DWORD sectors;
    WORD sector_size;
    DWORD block_size;
    FRESULT mount_result;
    FRESULT unmount_result;
    FRESULT open_write_result;
    FRESULT write_result;
    FRESULT sync_result;
    FRESULT close_result;
    FRESULT open_read_result;
    FRESULT read_result;
    UINT short_write_by;
    bool corrupt_readback;
    uint32_t fail_write_on_sequence;
    uint32_t disk_initialize_calls;
    uint32_t disk_ioctl_calls;
    uint32_t mount_calls;
    uint32_t unmount_calls;
    uint32_t open_calls;
    uint32_t write_calls;
    uint32_t read_calls;
    uint32_t force_not_ready_calls;
    user_spi_sd_diag_t diag;
    char file_data[256];
    size_t file_len;
} fake_storage_t;

extern fake_storage_t fake_storage;
void fake_storage_reset(void);

#endif
