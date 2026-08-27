/* Availability probe: measure how long the ECU holds zero authority after a
 * single lost power bundle, at the AMS's actual 10 Hz publication rate.
 * Uses the repository's own frame fixtures so the encoding is exact. */

#define main suppressed_consumer_main
#include "ams_power_consumer_test.c"
#undef main

#include <stdio.h>

#define PERIOD_MS DER26_POWER_NOMINAL_PUBLICATION_PERIOD_MS

static int fails;
static void ck(const char *n, int ok)
{
    if(!ok) { printf("  FAIL: %s\n", n); fails++; }
}

/* Returns milliseconds of continuous zero/invalid authority following a
 * dropped bundle at cycle `drop_cycle`, sampling every 10 ms. */
static uint32_t measure_outage(uint32_t drops)
{
    const uint8_t vf = DER26_POWER_FLAG_VALID | DER26_POWER_FLAG_AUTHORITY_VALID;
    der26_power_consumer_t c;
    der26_power_immediate_authority_t a;
    uint8_t payload[DER26_POWER_CORE_FRAME_COUNT][8];
    uint8_t counter = 0u;
    uint32_t t = 1000u;
    uint32_t outage_start = 0u, outage_ms = 0u;
    bool was_valid = false;

    der26_power_consumer_init(&c);

    /* Qualify: enough good bundles to reach steady valid authority. */
    for(int i = 0; i < 6; i++)
    {
        make_bundle(counter, payload, vf, vf);
        ingest_bundle(&c, payload, t);
        counter = (uint8_t)((counter + 1u) & 0x0Fu);
        t += PERIOD_MS;
    }
    was_valid = der26_power_consumer_get_immediate_authority(&c, t, &a);
    ck("qualified before drop", was_valid);

    /* Drop `drops` consecutive bundles: the AMS still increments its counter. */
    for(uint32_t d = 0; d < drops; d++)
    {
        counter = (uint8_t)((counter + 1u) & 0x0Fu);
        /* sample across the gap */
        for(uint32_t s = 0; s < PERIOD_MS; s += 10u)
        {
            bool ok = der26_power_consumer_get_immediate_authority(&c, t + s, &a);
            if(was_valid && !ok) { outage_start = t + s; }
            if(!ok && outage_start == 0u) { outage_start = t + s; }
            was_valid = ok;
        }
        t += PERIOD_MS;
    }

    /* Resume a clean stream and find when authority returns. */
    for(int i = 0; i < 12; i++)
    {
        make_bundle(counter, payload, vf, vf);
        ingest_bundle(&c, payload, t);
        counter = (uint8_t)((counter + 1u) & 0x0Fu);

        for(uint32_t s = 0; s < PERIOD_MS; s += 10u)
        {
            bool ok = der26_power_consumer_get_immediate_authority(&c, t + s, &a);
            if(ok && outage_start != 0u && outage_ms == 0u)
            {
                outage_ms = (t + s) - outage_start;
            }
            if(!ok && outage_start == 0u) { outage_start = t + s; }
        }
        t += PERIOD_MS;
    }
    return outage_ms;
}

int main(void)
{
    printf("== power-bundle availability probe (10 Hz publication) ==\n");

    uint32_t o1 = measure_outage(1u);
    printf("  1 dropped bundle  -> zero-authority outage = %u ms\n",
           (unsigned)o1);
    uint32_t o2 = measure_outage(2u);
    printf("  2 dropped bundles -> zero-authority outage = %u ms\n",
           (unsigned)o2);

    printf("  [context] MAX_AGE=%u ms  REQUIRED_GOOD_BUNDLES=%u  period=%u ms\n",
           (unsigned)DER26_POWER_MAX_AGE_MS,
           (unsigned)DER26_POWER_REQUIRED_GOOD_BUNDLES,
           (unsigned)PERIOD_MS);

    ck("single-drop outage is within the approved software budget",
       o1 <= DER26_POWER_SINGLE_DROP_OUTAGE_BUDGET_MS);
    ck("two-drop outage is within the approved software budget",
       o2 <= DER26_POWER_TWO_DROP_OUTAGE_BUDGET_MS);
    printf(fails ? "RESULT: %d FAILURES\n" : "RESULT: PASS (budget enforced)\n",
           fails);
    return fails ? 1 : 0;
}
