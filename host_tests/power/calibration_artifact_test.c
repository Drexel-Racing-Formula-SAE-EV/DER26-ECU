#include "power/ecu_pack_current_calibration.h"
#include "power/ecu_pack_current_model.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define EXPECT_TRUE(x) do{if(!(x)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)
#define EXPECT_FALSE(x) EXPECT_TRUE(!(x))
#define EXPECT_EQ_U32(a,e) do{uint32_t aa=(uint32_t)(a),ee=(uint32_t)(e);if(aa!=ee){printf("FAIL %s:%d: %s=%lu expected=%lu\n",__FILE__,__LINE__,#a,(unsigned long)aa,(unsigned long)ee);failures++;}}while(0)

int main(void)
{
    const ecu_pack_current_calibration_t *c = &g_ecu_pack_current_calibration;
    EXPECT_EQ_U32(c->magic, ECU_CURRENT_MODEL_MAGIC);
    EXPECT_EQ_U32(c->schema_version, ECU_CURRENT_MODEL_SCHEMA_VERSION);
    EXPECT_EQ_U32(c->torque_axis_points, 0u);
    EXPECT_EQ_U32(c->steady_cell_count, 0u);
    EXPECT_EQ_U32(c->transition_cell_count, 0u);
    EXPECT_FALSE(c->evidence_valid);
    EXPECT_EQ_U32(c->crc32, 0u);

    ecu_pack_current_calibration_runtime_t runtime;
    memset(&runtime, 0xA5, sizeof(runtime));
    EXPECT_FALSE(ecu_pack_current_calibration_qualify(c, 1u, &runtime));
    EXPECT_FALSE(ecu_pack_current_calibration_runtime_valid(&runtime));

    ecu_pack_current_calibration_t copy = *c;
    copy.evidence_valid = true;
    copy.crc32 = ecu_pack_current_calibration_crc32(&copy);
    EXPECT_FALSE(ecu_pack_current_calibration_qualify(&copy, 2u, &runtime));
    EXPECT_FALSE(ecu_pack_current_calibration_runtime_valid(&runtime));

    if(failures != 0)
    {
        printf("CALIBRATION ARTIFACT TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("PASS checked-in current calibration remains intentionally non-authoritative\n");
    return 0;
}
