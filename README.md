# DER26 ECU Firmware v2.6.3

STM32F767ZI / FreeRTOS firmware for the DER26 vehicle ECU.

Version 2.6.3 closes the Revision 7 audit items on top of the v2.6.2 target-hardening release. A latched pack-current residual now has an explicit protected-commit reason and, because no independently calibrated degraded torque cap exists, deterministically forces zero/disable. The independent Revision 7 invariant probe and measured power-bundle availability probe are permanent CI gates, with explicit 10 Hz outage budgets and a counter-wrap/staleness compile-time guard. The ECU remains independent of whether the AMS canonical current source is DHAB or APM.

Version 2.6.2 converted all application RTOS objects to static allocation, fixed residual persistence so it advances only on distinct AMS physical samples, added physical-sample metadata support, replaced APPS/brake sample counters with elapsed-time checks, instrumented clamp WCET with the Cortex-M7 DWT counter, and expanded the vehicle release gates.

**This is implemented software, not a released vehicle calibration.** The checked-in current-model artifact is deliberately invalid, current-measurement uncertainty remains provisional, cooling remains unvalidated, and all vehicle evidence gates default to `0`. The normal repository configuration cannot grant vehicle torque.

## Release locks

Bench builds use:

```text
ECU_BUILD_PROFILE=0
```

They run sensing, CAN parsing, candidate generation, the clamp, diagnostics, and host-testable safety logic while keeping `Firmware_Ok`, `MTR_EN`, `Cascadia_ON`, and enabled/nonzero torque inhibited.

A vehicle build requires the three top-level contract acknowledgements plus independent evidence for the pin map, APPS, brake sensing, discrete inputs, AMS protocol, current model, residual monitor, cooling, RTOS memory, WCET, CAN load, watchdog, and safe outputs. All gates default to zero.

```text
ECU_BUILD_PROFILE=1
ECU_BSPD_INTERFACE_3V3_VALIDATED=1
ECU_CM200_CAN_CONTRACT_VALIDATED=1
ECU_AMS_POWER_CLAMP_VALIDATED=1
ECU_PINMAP_VALIDATED=1
ECU_APPS_CALIBRATION_VALIDATED=1
ECU_BSE_CALIBRATION_VALIDATED=1
ECU_DISCRETE_INPUTS_VALIDATED=1
ECU_AMS_PROTOCOL_VALIDATED=1
ECU_CURRENT_MODEL_VALIDATED=1
ECU_CURRENT_RESIDUAL_VALIDATED=1
ECU_COOLING_VALIDATED=1
ECU_RTOS_MEMORY_VALIDATED=1
ECU_WCET_VALIDATED=1
ECU_CAN_LOAD_VALIDATED=1
ECU_WATCHDOG_VALIDATED=1
ECU_SAFE_OUTPUTS_VALIDATED=1
```

The source-owned implementation latch is now:

```text
ECU_AMS_POWER_CLAMP_IMPLEMENTED=1
```

That only states that the source path exists and passes software contract tests. It does not replace calibration, target timing, HIL, dyno, or vehicle evidence.

The BSPD board exports a nominal 12 V active-high `BSPD Ok` signal, while PE13 is a 3.3 V MCU input with no shown level conversion. Never connect that output directly to PE13. The CM200 acknowledgement also remains dependent on measured EEPROM/CAN configuration and timeout behavior.

## Torque-to-pack-current architecture

The APPS task runs at 100 Hz and produces a bounded torque-command contract. It does not update physical command state. The CAN task waits for a free hardware mailbox, takes the newest coherent AMS/CM200 snapshot, performs comparison-only re-verification using cached current intervals, builds the final CM200 packet, and updates clamp state only after `HAL_CAN_AddTxMessage()` accepts the packet.

The implementation includes:

- positive pack current = accumulator discharge;
- exact zero/DCL/CCL comparisons with numerical uncertainty applied by outward interval widening;
- source-independent AMS DCL, CCL, direction authorization, and canonical measured current;
- raw torque separated from zero/sign classification;
- zero-band hysteresis and persistent last-nonzero sign;
- physical-zero confirmation before opposite-sign torque;
- whole-cell-aligned increase search using constant certified cell bounds;
- bounded reduction search toward zero;
- separate absolute transition envelopes indexed by command profile, direction, full active raw span, speed, Vdc, and temperature region;
- bounded transition refinement for optional increases;
- settled tracking, microstep margin, anchor-deviation guard, cumulative-drift guard, settling time, and re-anchoring;
- asymmetric age-derived speed uncertainty for acceleration and deceleration/regrip;
- union across all touched torque and operating regions with fail-closed region caps;
- one provisional healthy-R2D auxiliary interval `[0, 0.250 A]`, applied once;
- battery-authority states including `LOW`, `TORQUE_EXHAUSTED`, and `ZERO_STEADY_AUX_INFEASIBLE`;
- execution-count bounds tied to the accepted 21-point/20-cell schema;
- no model call or search in the final protected mailbox commit;
- late authority/capability/operating-point changes producing zero;
- aggregate DHAB/APM residual monitoring with source-epoch handling.

See [Torque-to-pack-current clamp](Core/docs/ECU_TORQUE_TO_PACK_CURRENT_CLAMP.md) and [Certification campaign](Core/docs/ECU_CURRENT_MODEL_CERTIFICATION_CAMPAIGN.md).

## AMS supervision

Torque authorization requires independently fresh compact status/electrical/thermal/health data and a fresh coherent protocol-v2 authority bundle. Partial advisory updates may only tighten authority and do not refresh coherent freshness. The consumer uses the proven 4-bit ordering rule only inside its timing trust window.

The ECU does not branch on DHAB versus APM. Source identity is diagnostic only; equivalent canonical AMS data must produce identical clamp behavior. Live AMS v0.3.5 producer/ECU consumer compatibility is covered by `make power-source-compat`. The source diagnostic frame is advisory only and cannot change the clamp equations or grant authority.

## CM200 supervision

Required feedback frames remain `0x0A5`, `0x0A7`, `0x0AA`, `0x0AB`, `0x0AC`, and `0x0B1`, each with a 250 ms software timeout. Torque also requires CAN torque mode, no enable lockout, no POST/RUN fault, progressing counter/timer integrity, VSM Ready or Motor Running, and fresh torque capability.

A one-slot latest-value queue prevents an unsent old positive command from surviving a newer disable request. The actual hardware-mailbox commit is the state-transition point used by the clamp.

## Residual monitoring

The runtime monitor compares time-aligned canonical AMS pack current against the interval published for the torque actually accepted by bxCAN. It distinguishes step transition, slew tracking, settling, and steady phases. Stale/invalid current samples count as violations. Source-epoch changes reset persistence and enter bounded source settling without clearing a latched fault.

AMS frame `0x68B` now carries source, quality, boundary, source epoch, sample sequence, and physical sample age. The ECU reconstructs the physical sample tick and advances residual persistence only when the source sample sequence changes. If a system has never advertised `0x68B`, the legacy electrical-frame timestamp remains a compatibility fallback; after `0x68B` is observed, stale or incoherent metadata invalidates the residual measurement. Automatic mid-run source failover remains prohibited.

Because auxiliary current is not independently measured, violations are classified as total pack-current envelope violations rather than automatically as inverter-model failures.

The CLI `power` command reports clamp reason, authority state, path/sign/phase, model-call counts, predicted interval, residual status, source epoch, calibration qualification, and deadline overruns.

## Validation commands

From `host_tests/`:

```bash
make CC=clang clean
make CC=clang CLANG=clang clang-ci
make CC=clang asan
make CC=clang ubsan
make CC=clang stress
make CC=clang power-source-compat AMS_ROOT=/path/to/DER26-AMS/AMS
```

The current-model-specific targets are:

```bash
make CC=clang torque-clamp
make CC=clang residual-monitor
make CC=clang elapsed-timer
make CC=clang current-model-differential
```

Full application host syntax checking:

```bash
CC=clang bash ci/scripts/check_core_host_syntax.sh
```

Team CI may run the same portable suites with GCC and performs a bench ARM-GCC build. The local completion environment used for this release did not provide or attempt a GCC/ARM target build.

## Deliberate release blockers

The following remain open and keep `ECU_AMS_POWER_CLAMP_VALIDATED=0`:

- certified steady whole-cell current bounds;
- low-speed/stall and four-quadrant characterization;
- certified transition and composed-sequence envelopes;
- certified microstep margin and settling times;
- final auxiliary-current and DHAB/APM return-path boundary evidence;
- target STM32F767 WCET, stack, ISR-interference, and map measurements;
- CM200 timeout/EEPROM verification;
- HIL, dyno, holdout, and restricted vehicle testing;
- APM-primary validation and formal DHAB-removal decision.
