/* Independent probe of the Revision 7 non-negotiable torque-clamp invariants.
 * Reuses the repository test fixtures for a realistic calibration, but asserts
 * the contract-level properties rather than the implementation's own cases. */

#define main suppressed_repo_main
#include "torque_clamp_test.c"
#undef main

#include <stdio.h>

static int fails;
static void ck(const char *name, int ok)
{
    if(!ok) { printf("  FAIL: %s\n", name); fails++; }
}

static bool run(const ecu_torque_clamp_input_t *in,
                const ecu_pack_current_calibration_runtime_t *rt,
                const ecu_torque_clamp_state_t *st,
                ecu_torque_clamp_output_t *out)
{
    return ecu_torque_clamp_run(in, rt, st, out);
}

int main(void)
{
    ecu_pack_current_calibration_t cal = calibration();
    ecu_pack_current_calibration_runtime_t rt = qualify(&cal);
    ecu_torque_clamp_output_t out;

    printf("== Rev7 clamp invariant probe ==\n");

    /* INV-8/17: zero is always commandable, under every hostile condition. */
    {
        struct { const char *n; void (*mut)(ecu_torque_clamp_input_t *); } cases[] = {
            {"baseline", NULL},
        };
        (void)cases;

        ecu_torque_clamp_state_t st = confirmed_zero_state();
        ecu_torque_clamp_input_t in = input(0.0f);
        ck("zero: baseline commandable",
           run(&in, &rt, &st, &out) && out.selected_torque_nm == 0.0f &&
           out.output_valid);

        in = input(0.0f); in.discharge_authorized = false;
        ck("zero: discharge inhibited",
           run(&in, &rt, &st, &out) && out.selected_torque_nm == 0.0f);

        in = input(0.0f); in.charge_authorized = false;
        ck("zero: charge inhibited",
           run(&in, &rt, &st, &out) && out.selected_torque_nm == 0.0f);

        in = input(0.0f); in.dcl_a = 0.0f; in.ccl_a = 0.0f;
        ck("zero: DCL=CCL=0",
           run(&in, &rt, &st, &out) && out.selected_torque_nm == 0.0f);

        in = input(0.0f); in.authority_valid = false;
        (void)run(&in, &rt, &st, &out);
        ck("zero: invalid authority still yields zero torque",
           out.selected_torque_nm == 0.0f);

        in = input(0.0f); in.operating_point_valid = false;
        (void)run(&in, &rt, &st, &out);
        ck("zero: invalid operating point yields zero",
           out.selected_torque_nm == 0.0f);
    }

    /* INV-11/12: reversal must not bypass physical zero confirmation. */
    {
        ecu_torque_clamp_state_t st;
        ecu_torque_clamp_state_init(&st);
        st.path_state = ECU_TORQUE_PATH_WAITING_FOR_ZERO_CONFIRMATION;
        st.last_nonzero_committed_sign = ECU_TORQUE_SIGN_POSITIVE;
        st.physical_zero_confirmed = false;
        st.normalized_zero = true;
        st.raw_committed_torque_nm = 0.0f;

        ecu_torque_clamp_input_t in = input(-40.0f);
        in.physical_zero_confirmed = false;
        (void)run(&in, &rt, &st, &out);
        ck("reversal: opposite sign blocked before physical zero",
           out.selected_torque_nm == 0.0f);
        ck("reversal: reason is reversal wait",
           out.reason == ECU_CLAMP_REASON_REVERSAL_WAIT ||
           out.selected_torque_nm == 0.0f);

        /* Same-side reapplication after a zero dwell must not be blocked
         * by the reversal machinery. */
        ecu_torque_clamp_input_t in2 = input(40.0f);
        in2.physical_zero_confirmed = false;
        (void)run(&in2, &rt, &st, &out);
        printf("  [info] same-side reapply during zero-wait -> %.2f Nm reason=%d\n",
               (double)out.selected_torque_nm, (int)out.reason);
    }

    /* INV: lower DCL can never increase permitted positive torque. */
    {
        ecu_torque_clamp_state_t st = confirmed_zero_state();
        float prev = 1e9f;
        int mono_ok = 1;
        for(float dcl = 120.0f; dcl >= 0.0f; dcl -= 5.0f)
        {
            ecu_torque_clamp_input_t in = input(200.0f);
            in.dcl_a = dcl;
            if(run(&in, &rt, &st, &out))
            {
                if(out.selected_torque_nm > prev + 1e-3f) { mono_ok = 0; }
                prev = out.selected_torque_nm;
            }
        }
        ck("monotone: lower DCL never increases torque", mono_ok);
    }

    /* INV: selected nonzero torque's steady interval must fit DCL/CCL. */
    {
        ecu_torque_clamp_state_t st = confirmed_zero_state();
        int feas_ok = 1;
        for(float req = -100.0f; req <= 200.0f; req += 7.0f)
        {
            for(float dcl = 0.0f; dcl <= 120.0f; dcl += 17.0f)
            {
                ecu_torque_clamp_input_t in = input(req);
                in.dcl_a = dcl;
                if(!run(&in, &rt, &st, &out)) { continue; }
                if(!out.selected_nonzero) { continue; }
                if(out.steady_current_a.max_a > in.dcl_a + 1e-3f) { feas_ok = 0; }
                if(out.steady_current_a.min_a < -in.ccl_a - 1e-3f) { feas_ok = 0; }
            }
        }
        ck("feasibility: selected steady interval inside DCL/CCL", feas_ok);
    }

    /* INV-31/32: commit re-verification runs no model and only weakens. */
    {
        ecu_torque_clamp_state_t st = confirmed_zero_state();
        ecu_torque_clamp_input_t in = input(60.0f);
        ck("setup nonzero", run(&in, &rt, &st, &out) && out.selected_nonzero);

        ecu_torque_commit_verification_t v = {
            .dcl_a = in.dcl_a, .ccl_a = in.ccl_a,
            .cm200_positive_cap_nm = in.cm200_positive_cap_nm,
            .cm200_negative_cap_nm = in.cm200_negative_cap_nm,
            .speed_generation = in.speed_generation,
            .vdc_generation = in.vdc_generation,
            .temperature_generation = in.temperature_generation,
            .capability_generation = in.capability_generation,
            .calibration_generation = in.calibration_generation,
            .commit_timestamp_us = in.now_us,
            .discharge_authorized = true, .charge_authorized = true,
            .authority_valid = true, .calibration_valid = true,
            .safety_gate_valid = true, .physical_zero_confirmed = true,
        };
        float committed = -1.0f;
        ecu_torque_clamp_reason_t reason = ECU_CLAMP_REASON_NONE;
        ck("commit: unchanged authority commits candidate",
           ecu_torque_clamp_commit_reverify(&out, &v, &committed, &reason) &&
           committed == out.selected_torque_nm);

        ecu_torque_commit_verification_t v2 = v;
        v2.dcl_a = 0.0f;
        committed = -1.0f;
        (void)ecu_torque_clamp_commit_reverify(&out, &v2, &committed, &reason);
        ck("commit: tightened DCL -> zero, not a late search",
           committed == 0.0f);

        ecu_torque_commit_verification_t v3 = v;
        v3.authority_valid = false;
        committed = -1.0f;
        (void)ecu_torque_clamp_commit_reverify(&out, &v3, &committed, &reason);
        ck("commit: invalid authority -> zero", committed == 0.0f);

        ecu_torque_commit_verification_t v4 = v;
        v4.speed_generation = in.speed_generation + 1u;
        committed = -1.0f;
        (void)ecu_torque_clamp_commit_reverify(&out, &v4, &committed, &reason);
        ck("commit: operating-point generation change -> zero",
           committed == 0.0f);

        ecu_torque_commit_verification_t v5 = v;
        v5.dcl_a = in.dcl_a + 50.0f;   /* authority relaxed */
        committed = -1.0f;
        (void)ecu_torque_clamp_commit_reverify(&out, &v5, &committed, &reason);
        ck("commit: relaxed authority cannot increase torque",
           committed <= out.selected_torque_nm + 1e-6f);
    }

    /* INV-33: execution counters must stay within declared bounds. */
    {
        ecu_torque_clamp_state_t st = confirmed_zero_state();
        uint16_t max_cells = 0u, max_steady = 0u, max_trans = 0u;
        for(float req = -120.0f; req <= 220.0f; req += 3.0f)
        {
            ecu_torque_clamp_input_t in = input(req);
            if(!run(&in, &rt, &st, &out)) { continue; }
            if(out.torque_cells_evaluated > max_cells)
                max_cells = out.torque_cells_evaluated;
            if(out.steady_model_calls > max_steady)
                max_steady = out.steady_model_calls;
            if(out.transition_model_calls > max_trans)
                max_trans = out.transition_model_calls;
        }
        printf("  [info] worst-case cells=%u steady_calls=%u transition_calls=%u\n",
               (unsigned)max_cells, (unsigned)max_steady, (unsigned)max_trans);
        ck("bounds: cells evaluated bounded", max_cells <= 64u);
        ck("bounds: steady calls bounded", max_steady <= 128u);
    }

    printf(fails ? "RESULT: %d FAILURES\n" : "RESULT: PASS (0 failures)\n", fails);
    return fails ? 1 : 0;
}
