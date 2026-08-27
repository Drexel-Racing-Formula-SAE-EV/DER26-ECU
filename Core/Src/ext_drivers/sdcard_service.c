#include "ext_drivers/sdcard_service.h"
#include "fatfs.h"
#include "diskio.h"
#include <string.h>
#include <stdio.h>

#ifndef ECU_HOST_TEST
#include "FreeRTOS.h"
#include "semphr.h"
static StaticSemaphore_t fs_mutex_buffer;
static SemaphoreHandle_t fs_mutex;
#endif

static sdcard_diag_t g;
static FATFS fs;

bool sdcard_service_lock(uint32_t timeout_ms)
{
#ifdef ECU_HOST_TEST
    (void)timeout_ms;
    return true;
#else
    if(fs_mutex == NULL) return false;
    TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY :
                       pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(fs_mutex, ticks) == pdTRUE;
#endif
}

void sdcard_service_unlock(void)
{
#ifndef ECU_HOST_TEST
    if(fs_mutex != NULL) (void)xSemaphoreGiveRecursive(fs_mutex);
#endif
}

static void record_result(FRESULT r)
{
    g.last_fresult = r;
    if (r != FR_OK) g.last_error = r;
}

static void refresh_protocol(void) { USER_SPI_get_diag(&g.protocol); }
static void clear_geometry(void)
{
    g.sector_count = 0u;
    g.sector_size = 0u;
    g.block_size = 0u;
}

const char *sdcard_fresult_name(FRESULT r)
{
    static const char *n[] = {
        "FR_OK","FR_DISK_ERR","FR_INT_ERR","FR_NOT_READY","FR_NO_FILE",
        "FR_NO_PATH","FR_INVALID_NAME","FR_DENIED","FR_EXIST",
        "FR_INVALID_OBJECT","FR_WRITE_PROTECTED","FR_INVALID_DRIVE",
        "FR_NOT_ENABLED","FR_NO_FILESYSTEM","FR_MKFS_ABORTED","FR_TIMEOUT",
        "FR_LOCKED","FR_NOT_ENOUGH_CORE","FR_TOO_MANY_OPEN_FILES",
        "FR_INVALID_PARAMETER"
    };
    return ((unsigned)r < (sizeof(n) / sizeof(n[0]))) ? n[r] : "FR_UNKNOWN";
}

void sdcard_service_init(void)
{
#ifndef ECU_HOST_TEST
    fs_mutex = xSemaphoreCreateRecursiveMutexStatic(&fs_mutex_buffer);
#endif
    memset(&g, 0, sizeof(g));
    MX_FATFS_Init();
    g.linked = (retUSER == 0u);
    g.disk_status = STA_NOINIT;
    g.last_error = FR_OK;
    record_result(g.linked ? FR_OK : FR_INVALID_DRIVE);
    refresh_protocol();
}

void sdcard_service_get(sdcard_diag_t *out)
{
    if(!sdcard_service_lock(100u)) return;
    refresh_protocol();
    if (out != NULL) *out = g;
    sdcard_service_unlock();
}

FRESULT sdcard_service_card_init(void)
{
    DWORD sectors = 0u;
    WORD sector_size = 0u;
    DWORD block_size = 0u;
    FRESULT result = FR_OK;

    if(!sdcard_service_lock(1500u)) return FR_TIMEOUT;
    g.init_attempts++;
    g.mounted = false;
    g.initialized = false;
    clear_geometry();

    if (!g.linked) {
        g.disk_status = STA_NOINIT;
        g.failures++;
        result = FR_INVALID_DRIVE;
        goto out;
    }

    g.disk_status = disk_initialize(0u);
    refresh_protocol();
    if ((g.disk_status & STA_NOINIT) != 0u) {
        g.failures++;
        result = FR_NOT_READY;
        goto out;
    }

    if (disk_ioctl(0u, GET_SECTOR_COUNT, &sectors) != RES_OK || sectors == 0u ||
        disk_ioctl(0u, GET_SECTOR_SIZE, &sector_size) != RES_OK || sector_size != 512u) {
        USER_SPI_force_not_ready();
        refresh_protocol();
        g.disk_status = disk_status(0u);
        g.failures++;
        result = FR_NOT_READY;
        goto out;
    }

    if (disk_ioctl(0u, GET_BLOCK_SIZE, &block_size) != RES_OK || block_size == 0u)
        block_size = 1u;

    g.sector_count = sectors;
    g.sector_size = sector_size;
    g.block_size = block_size;
    g.disk_status = disk_status(0u);
    g.initialized = ((g.disk_status & STA_NOINIT) == 0u);
    if (!g.initialized) {
        clear_geometry();
        g.failures++;
        result = FR_NOT_READY;
    }

out:
    record_result(result);
    sdcard_service_unlock();
    return result;
}

FRESULT sdcard_service_mount(void)
{
    FRESULT r;
    if(!sdcard_service_lock(2000u)) return FR_TIMEOUT;

    if (!g.initialized || (disk_status(0u) & STA_NOINIT) != 0u) {
        r = sdcard_service_card_init();
        if (r != FR_OK) goto out;
    }

    g.mount_attempts++;
    r = f_mount(&fs, USERPath, 1u);
    g.mounted = (r == FR_OK);
    g.disk_status = disk_status(0u);
    if (!g.mounted) g.failures++;
    record_result(r);
out:
    sdcard_service_unlock();
    return r;
}

FRESULT sdcard_service_unmount(void)
{
    if(!sdcard_service_lock(1000u)) return FR_TIMEOUT;
    FRESULT r = f_mount(NULL, USERPath, 0u);
    g.mounted = false;
    g.disk_status = disk_status(0u);
    record_result(r);
    sdcard_service_unlock();
    return r;
}

FRESULT sdcard_service_write_read_test(const char *name, uint32_t sequence)
{
    FIL f;
    UINT bw = 0u, br = 0u;
    char tx[128], rx[128];
    FRESULT r;

    if(!sdcard_service_lock(3000u)) return FR_TIMEOUT;
    if (!g.mounted) {
        r = sdcard_service_mount();
        if (r != FR_OK) goto out_no_failure_increment;
    }
    if (name == NULL || *name == '\0') name = "sdtest.txt";

    (void)snprintf(tx, sizeof(tx), "DER26 ECU SD test sequence=%lu\r\n",
                   (unsigned long)sequence);
    memset(rx, 0, sizeof(rx));

    r = f_open(&f, name, FA_CREATE_ALWAYS | FA_WRITE);
    if (r != FR_OK) goto fail;
    r = f_write(&f, tx, (UINT)strlen(tx), &bw);
    if (r == FR_OK && bw == strlen(tx)) r = f_sync(&f);
    else if (r == FR_OK) r = FR_DISK_ERR;
    {
        FRESULT c = f_close(&f);
        if (r == FR_OK) r = c;
    }
    if (r != FR_OK) goto fail;

    r = f_open(&f, name, FA_READ);
    if (r != FR_OK) goto fail;
    r = f_read(&f, rx, sizeof(rx) - 1u, &br);
    {
        FRESULT c = f_close(&f);
        if (r == FR_OK) r = c;
    }
    if (r == FR_OK && br == strlen(tx) && memcmp(tx, rx, br) == 0) {
        g.write_pass++;
        g.read_pass++;
        record_result(FR_OK);
        sdcard_service_unlock();
        return FR_OK;
    }
    if (r == FR_OK) r = FR_DISK_ERR;

fail:
    g.failures++;
    record_result(r);
out_no_failure_increment:
    sdcard_service_unlock();
    return r;
}

FRESULT sdcard_service_soak(uint32_t count, uint32_t *passed)
{
    uint32_t p = 0u;
    FRESULT r = FR_OK;
    if (count == 0u || count > 1000u) {
        if(sdcard_service_lock(100u)) {
            record_result(FR_INVALID_PARAMETER);
            sdcard_service_unlock();
        }
        return FR_INVALID_PARAMETER;
    }
    for (uint32_t i = 0u; i < count; i++) {
        r = sdcard_service_write_read_test("sdsoak.txt", i);
        if (r != FR_OK) break;
        p++;
    }
    if (passed != NULL) *passed = p;
    return r;
}
