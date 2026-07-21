# ECU Host Tests

These suites exercise pure ECU safety/parser logic without STM32 hardware. They do not prove pin levels, interrupt timing, physical CAN behavior, target ABI, or controller configuration.

Current coverage includes:

- legacy AMS bounds/layout/tail parsing for all 75 cells, temperature groups, and fans;
- compact AMS `0x680-0x683` decoding, protocol/sequence/freshness/fault gates, physical plausibility, and immediate invalidation of malformed required frames;
- CM200 little-endian decoding for all supported broadcasts;
- CM200 required-frame freshness, command-counter acquisition/progression/mismatch, torque echo, timer progression/reset, fault words, VSM torque readiness, and capability clamp;
- split CM200 feedback-health versus torque-ready behavior through precharge;
- CM200 startup grace, runtime loss, immediate fault, latching, and timer-wrap policy;
- signed command encoding, direction-preserving disable, rolling counter, unlock sequence, packet inspection, and torque slew limiting;
- RTD release-before-arm, early/stuck press rejection, momentary-button behavior, sound timing, fault exit, rearm, and tick wrap;
- BPSD active-high healthy semantics and delayed healthy recovery;
- complete torque-gate fault matrix and task-heartbeat wrap behavior;
- deterministic AMS fault/sequence fuzz and extended stress runs;
- bench/vehicle compile locks for both BPSD and CM200 acknowledgements;
- GCC static analysis and ASan/UBSan for every host suite.

Run from `host_tests/`:

```sh
make CC=gcc clean
make CC=gcc unit
make CC=gcc test
make CC=gcc system-sil
make CC=gcc profile-gates
make CC=gcc analyze
make CC=gcc asan
make CC=gcc ubsan
make CC=gcc stress
```

`make ci` runs unit, regression, system SIL, profile gates, and GCC analysis. The repository CI additionally type-checks every application source in the inhibited bench profile and the fully acknowledged vehicle profile, then performs the ARM-GCC target build when that toolchain is available.
