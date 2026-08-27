#include "storage/fake_storage.h"
#include "ext_drivers/sdcard_service.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define EXPECT_TRUE(x) do{if(!(x)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)
#define EXPECT_FALSE(x) EXPECT_TRUE(!(x))
#define EXPECT_EQ_U32(a,e) do{uint32_t aa=(uint32_t)(a),ee=(uint32_t)(e);if(aa!=ee){printf("FAIL %s:%d: %s=%lu expected=%lu\n",__FILE__,__LINE__,#a,(unsigned long)aa,(unsigned long)ee);failures++;}}while(0)
#define EXPECT_EQ_I32(a,e) do{int32_t aa=(int32_t)(a),ee=(int32_t)(e);if(aa!=ee){printf("FAIL %s:%d: %s=%ld expected=%ld\n",__FILE__,__LINE__,#a,(long)aa,(long)ee);failures++;}}while(0)

static void reset_service(void)
{
    fake_storage_reset();
    sdcard_service_init();
}

static sdcard_diag_t diag(void)
{
    sdcard_diag_t d;
    memset(&d, 0xA5, sizeof(d));
    sdcard_service_get(&d);
    return d;
}

static void test_link_and_no_card_fail_closed(void)
{
    fake_storage_reset();
    fake_storage.link_result = 1u;
    sdcard_service_init();
    sdcard_diag_t d = diag();
    EXPECT_FALSE(d.linked);
    EXPECT_EQ_I32(d.last_fresult, FR_INVALID_DRIVE);
    EXPECT_EQ_I32(sdcard_service_card_init(), FR_INVALID_DRIVE);

    reset_service();
    fake_storage.initialize_status = STA_NOINIT;
    EXPECT_EQ_I32(sdcard_service_card_init(), FR_NOT_READY);
    d = diag();
    EXPECT_FALSE(d.initialized);
    EXPECT_FALSE(d.mounted);
    EXPECT_TRUE((d.disk_status & STA_NOINIT) != 0u);
    EXPECT_EQ_U32(d.failures, 1u);
    EXPECT_EQ_I32(d.last_error, FR_NOT_READY);
}

static void test_geometry_validation_and_optional_block_fallback(void)
{
    reset_service();
    fake_storage.sectors = 0u;
    EXPECT_EQ_I32(sdcard_service_card_init(), FR_NOT_READY);
    EXPECT_EQ_U32(fake_storage.force_not_ready_calls, 1u);
    sdcard_diag_t d = diag();
    EXPECT_FALSE(d.initialized);
    EXPECT_EQ_U32(d.sector_count, 0u);

    reset_service();
    fake_storage.sector_size = 1024u;
    EXPECT_EQ_I32(sdcard_service_card_init(), FR_NOT_READY);
    EXPECT_EQ_U32(fake_storage.force_not_ready_calls, 1u);

    reset_service();
    fake_storage.block_size_result = RES_ERROR;
    EXPECT_EQ_I32(sdcard_service_card_init(), FR_OK);
    d = diag();
    EXPECT_TRUE(d.initialized);
    EXPECT_EQ_U32(d.sector_count, 65536u);
    EXPECT_EQ_U32(d.sector_size, 512u);
    EXPECT_EQ_U32(d.block_size, 1u);
}

static void test_mount_unmount_and_error_memory(void)
{
    reset_service();
    EXPECT_EQ_I32(sdcard_service_mount(), FR_OK);
    sdcard_diag_t d = diag();
    EXPECT_TRUE(d.initialized);
    EXPECT_TRUE(d.mounted);
    EXPECT_EQ_U32(d.init_attempts, 1u);
    EXPECT_EQ_U32(d.mount_attempts, 1u);

    fake_storage.unmount_result = FR_OK;
    EXPECT_EQ_I32(sdcard_service_unmount(), FR_OK);
    d = diag();
    EXPECT_FALSE(d.mounted);

    reset_service();
    fake_storage.mount_result = FR_NO_FILESYSTEM;
    EXPECT_EQ_I32(sdcard_service_mount(), FR_NO_FILESYSTEM);
    d = diag();
    EXPECT_FALSE(d.mounted);
    EXPECT_EQ_I32(d.last_error, FR_NO_FILESYSTEM);
    fake_storage.unmount_result = FR_OK;
    EXPECT_EQ_I32(sdcard_service_unmount(), FR_OK);
    d = diag();
    EXPECT_EQ_I32(d.last_fresult, FR_OK);
    EXPECT_EQ_I32(d.last_error, FR_NO_FILESYSTEM);
}

static void test_write_read_success_and_failures(void)
{
    reset_service();
    EXPECT_EQ_I32(sdcard_service_write_read_test("test.txt", 42u), FR_OK);
    sdcard_diag_t d = diag();
    EXPECT_TRUE(d.mounted);
    EXPECT_EQ_U32(d.write_pass, 1u);
    EXPECT_EQ_U32(d.read_pass, 1u);
    EXPECT_EQ_U32(d.failures, 0u);
    EXPECT_TRUE(strstr(fake_storage.file_data, "sequence=42") != NULL);

    reset_service();
    fake_storage.short_write_by = 1u;
    EXPECT_EQ_I32(sdcard_service_write_read_test("test.txt", 1u), FR_DISK_ERR);
    d = diag();
    EXPECT_EQ_U32(d.write_pass, 0u);
    EXPECT_EQ_U32(d.failures, 1u);

    reset_service();
    fake_storage.corrupt_readback = true;
    EXPECT_EQ_I32(sdcard_service_write_read_test("test.txt", 2u), FR_DISK_ERR);
    d = diag();
    EXPECT_EQ_U32(d.read_pass, 0u);
    EXPECT_EQ_U32(d.failures, 1u);

    reset_service();
    fake_storage.open_write_result = FR_DENIED;
    EXPECT_EQ_I32(sdcard_service_write_read_test(NULL, 3u), FR_DENIED);
    d = diag();
    EXPECT_EQ_I32(d.last_error, FR_DENIED);
}

static void test_soak_bounds_and_first_failure(void)
{
    uint32_t passed = 999u;
    reset_service();
    EXPECT_EQ_I32(sdcard_service_soak(0u, &passed), FR_INVALID_PARAMETER);
    EXPECT_EQ_U32(passed, 999u);
    EXPECT_EQ_I32(sdcard_service_soak(1001u, &passed), FR_INVALID_PARAMETER);

    reset_service();
    fake_storage.fail_write_on_sequence = 7u;
    EXPECT_EQ_I32(sdcard_service_soak(20u, &passed), FR_DISK_ERR);
    EXPECT_EQ_U32(passed, 7u);
    sdcard_diag_t d = diag();
    EXPECT_EQ_U32(d.write_pass, 7u);
    EXPECT_EQ_U32(d.read_pass, 7u);
    EXPECT_EQ_U32(d.failures, 1u);

    reset_service();
    EXPECT_EQ_I32(sdcard_service_soak(100u, &passed), FR_OK);
    EXPECT_EQ_U32(passed, 100u);
    d = diag();
    EXPECT_EQ_U32(d.write_pass, 100u);
    EXPECT_EQ_U32(d.read_pass, 100u);
}

static void test_fresult_names(void)
{
    EXPECT_TRUE(strcmp(sdcard_fresult_name(FR_OK), "FR_OK") == 0);
    EXPECT_TRUE(strcmp(sdcard_fresult_name(FR_NOT_READY), "FR_NOT_READY") == 0);
    EXPECT_TRUE(strcmp(sdcard_fresult_name(FR_NO_FILESYSTEM), "FR_NO_FILESYSTEM") == 0);
    EXPECT_TRUE(strcmp(sdcard_fresult_name((FRESULT)99), "FR_UNKNOWN") == 0);
}

static void run(const char *name, void (*fn)(void))
{
    int before = failures;
    fn();
    if(before == failures) printf("PASS %s\n", name);
}

int main(void)
{
    run("SD link and no-card fail closed", test_link_and_no_card_fail_closed);
    run("SD geometry validation", test_geometry_validation_and_optional_block_fallback);
    run("SD mount and persistent error memory", test_mount_unmount_and_error_memory);
    run("SD verified write/read", test_write_read_success_and_failures);
    run("SD soak boundaries and interruption", test_soak_bounds_and_first_failure);
    run("SD FRESULT names", test_fresult_names);
    if(failures != 0)
    {
        printf("SDCARD SERVICE TESTS FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("ALL SDCARD SERVICE TESTS PASSED\n");
    return 0;
}
