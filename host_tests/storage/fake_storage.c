#include "storage/fake_storage.h"
#include "fatfs.h"

#include <string.h>
#include <stdlib.h>

fake_storage_t fake_storage;
uint8_t retUSER;
char USERPath[4] = "0:";
FATFS USERFatFS;
FIL USERFile;

static bool write_mode;
static uint32_t parsed_sequence;

void fake_storage_reset(void)
{
    memset(&fake_storage, 0, sizeof(fake_storage));
    fake_storage.initialize_status = 0u;
    fake_storage.status = 0u;
    fake_storage.sector_count_result = RES_OK;
    fake_storage.sector_size_result = RES_OK;
    fake_storage.block_size_result = RES_OK;
    fake_storage.sectors = 65536u;
    fake_storage.sector_size = 512u;
    fake_storage.block_size = 128u;
    fake_storage.mount_result = FR_OK;
    fake_storage.unmount_result = FR_OK;
    fake_storage.open_write_result = FR_OK;
    fake_storage.write_result = FR_OK;
    fake_storage.sync_result = FR_OK;
    fake_storage.close_result = FR_OK;
    fake_storage.open_read_result = FR_OK;
    fake_storage.read_result = FR_OK;
    fake_storage.fail_write_on_sequence = UINT32_MAX;
    fake_storage.diag.stage = USER_SPI_SD_STAGE_IDLE;
    fake_storage.diag.cmd0_r1 = 0xFFu;
    fake_storage.diag.cmd8_r1 = 0xFFu;
    fake_storage.diag.acmd41_or_cmd1_r1 = 0xFFu;
    fake_storage.diag.cmd58_or_cmd16_r1 = 0xFFu;
    write_mode = false;
    parsed_sequence = 0u;
    retUSER = 0u;
    memcpy(USERPath, "0:", 3u);
}

void MX_FATFS_Init(void)
{
    retUSER = fake_storage.link_result;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    fake_storage.disk_initialize_calls++;
    if(pdrv != 0u) return STA_NOINIT;
    fake_storage.status = fake_storage.initialize_status;
    if((fake_storage.status & STA_NOINIT) == 0u)
    {
        fake_storage.diag.stage = USER_SPI_SD_STAGE_READY;
        fake_storage.diag.card_type = 0x0Cu;
        fake_storage.diag.csd_valid = 1u;
    }
    else
    {
        fake_storage.diag.stage = USER_SPI_SD_STAGE_FAILED;
    }
    return fake_storage.status;
}

DSTATUS disk_status(BYTE pdrv)
{
    return (pdrv == 0u) ? fake_storage.status : STA_NOINIT;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    fake_storage.disk_ioctl_calls++;
    if(pdrv != 0u || buff == NULL) return RES_PARERR;
    switch(cmd)
    {
    case GET_SECTOR_COUNT:
        if(fake_storage.sector_count_result == RES_OK)
            *(DWORD *)buff = fake_storage.sectors;
        return fake_storage.sector_count_result;
    case GET_SECTOR_SIZE:
        if(fake_storage.sector_size_result == RES_OK)
            *(WORD *)buff = fake_storage.sector_size;
        return fake_storage.sector_size_result;
    case GET_BLOCK_SIZE:
        if(fake_storage.block_size_result == RES_OK)
            *(DWORD *)buff = fake_storage.block_size;
        return fake_storage.block_size_result;
    default:
        return RES_PARERR;
    }
}

void USER_SPI_get_diag(user_spi_sd_diag_t *out)
{
    if(out != NULL) *out = fake_storage.diag;
}

void USER_SPI_force_not_ready(void)
{
    fake_storage.force_not_ready_calls++;
    fake_storage.status = STA_NOINIT;
    fake_storage.diag.stage = USER_SPI_SD_STAGE_FAILED;
    fake_storage.diag.card_type = 0u;
    fake_storage.diag.csd_valid = 0u;
}

const char *USER_SPI_stage_name(user_spi_sd_stage_t stage)
{
    (void)stage;
    return "HOST";
}

FRESULT f_mount(FATFS *fs, const TCHAR *path, BYTE opt)
{
    (void)path; (void)opt;
    if(fs == NULL)
    {
        fake_storage.unmount_calls++;
        return fake_storage.unmount_result;
    }
    fake_storage.mount_calls++;
    return fake_storage.mount_result;
}

FRESULT f_open(FIL *fp, const TCHAR *path, BYTE mode)
{
    (void)fp; (void)path;
    fake_storage.open_calls++;
    write_mode = ((mode & FA_WRITE) != 0u);
    if(write_mode)
    {
        fake_storage.file_len = 0u;
        fake_storage.file_data[0] = '\0';
        return fake_storage.open_write_result;
    }
    return fake_storage.open_read_result;
}

FRESULT f_write(FIL *fp, const void *buff, UINT btw, UINT *bw)
{
    (void)fp;
    fake_storage.write_calls++;
    if(fake_storage.write_result != FR_OK)
    {
        if(bw != NULL) *bw = 0u;
        return fake_storage.write_result;
    }
    const char *s = (const char *)buff;
    const char *marker = strstr(s, "sequence=");
    parsed_sequence = (marker != NULL) ? (uint32_t)strtoul(marker + 9, NULL, 10) : 0u;
    if(parsed_sequence == fake_storage.fail_write_on_sequence)
    {
        if(bw != NULL) *bw = 0u;
        return FR_DISK_ERR;
    }
    UINT actual = btw;
    if(fake_storage.short_write_by < actual) actual -= fake_storage.short_write_by;
    if(actual >= sizeof(fake_storage.file_data)) actual = sizeof(fake_storage.file_data) - 1u;
    memcpy(fake_storage.file_data, buff, actual);
    fake_storage.file_data[actual] = '\0';
    fake_storage.file_len = actual;
    if(bw != NULL) *bw = actual;
    return FR_OK;
}

FRESULT f_sync(FIL *fp)
{
    (void)fp;
    return fake_storage.sync_result;
}

FRESULT f_close(FIL *fp)
{
    (void)fp;
    return fake_storage.close_result;
}

FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br)
{
    (void)fp;
    fake_storage.read_calls++;
    if(fake_storage.read_result != FR_OK)
    {
        if(br != NULL) *br = 0u;
        return fake_storage.read_result;
    }
    size_t n = fake_storage.file_len;
    if(n > btr) n = btr;
    memcpy(buff, fake_storage.file_data, n);
    if(fake_storage.corrupt_readback && n > 0u)
        ((uint8_t *)buff)[0] ^= 0x01u;
    if(br != NULL) *br = (UINT)n;
    return FR_OK;
}
