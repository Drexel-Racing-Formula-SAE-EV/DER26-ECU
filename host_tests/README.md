# ECU Host Tests

These suites exercise portable ECU parser, safety, current-model, clamp, and residual-monitor logic without STM32 hardware. They do not prove pin levels, interrupt timing, target ABI, CM200 EEPROM configuration, physical CAN behavior, or vehicle calibration.

## Main coverage

- legacy and compact AMS decoding, freshness, sequence, physical plausibility, and immediate invalidation;
- protocol-v2 coherent DCL/CCL bundle and advisory handling;
- live AMS v0.3.6 producer-to-ECU-consumer compatibility;
- CM200 feedback decoding, freshness, counter/echo/timer integrity, capability, and fault gating;
- RTD, BSPD, heartbeat, torque-gate, and deterministic fault-injection SIL;
- torque/current schema and boot qualification;
- direct whole-cell path enumeration;
- input-age uncertainty and region-overflow behavior;
- zero hysteresis, sign history, reversal-through-zero prevention;
- full-span transition lookup and transition refinement;
- settled tracking, microstep margin, cumulative drift, settling, and re-anchor;
- battery-authority classification;
- comparison-only final commit verification;
- execution-count bounds tied to the maximum accepted schema;
- aggregate current residual monitoring and source-epoch transitions;
- independent Python/C current-model vectors;
- 20,000 randomized clamp contract iterations and extended 50,000-cycle SIL stress;
- ASan, UBSan, Clang static analysis, and build-profile gates.

## Recommended local commands

From `host_tests/`:

```sh
make CC=clang clean
make CC=clang CLANG=clang clang-ci
make CC=clang asan
make CC=clang ubsan
make CC=clang stress
```

Focused current-model targets:

```sh
make CC=clang torque-clamp
make CC=clang residual-monitor
make CC=clang current-model-differential
```

Protocol targets:

```sh
make CC=clang power-consumer
make CC=clang power-integration
make CC=clang power-golden
make CC=clang power-source-compat AMS_ROOT=/path/to/DER26-AMS/AMS
```

`power-source-compat` compiles the live AMS `ams_power_can.c` producer together with the ECU consumer, compares all six protocol-v2 payloads against locked vectors, and validates the decoded authority, envelope, and resource state. It requires a sibling AMS source tree and is intentionally separate from standalone `clang-ci`.

## CI notes

`make clang-ci` runs current-model contract tests, residual-monitor tests, independent Python/C vectors, legacy unit/regression suites, protocol-v2 conformance/integration/golden tests, system SIL, profile gates, and Clang analysis.

Team CI may run portable suites with GCC and performs a bench ARM-GCC build. A fully acknowledged vehicle source may parse because the implementation latch is enabled, but the default vehicle build remains blocked by external BSPD, CM200, and clamp-validation evidence. The checked-in numerical calibration remains deliberately invalid.
