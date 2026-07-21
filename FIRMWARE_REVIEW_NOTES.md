# ECU Firmware Review Notes — v2.3.0

Review sources included the DER26 ECU firmware, hardened AMS firmware/CAN contract, CM200 software manual, AMS/design documentation, standalone BPSD schematic/BOM, shutdown schematic, RTM design, ECU miscellaneous schematic, and MCU-breakout schematic.

## Closed in firmware

- AMS compact torque gate now requires fresh status, electrical, and thermal frames.
- AMS thermal fatal/invalid flags and summary sanity are enforced.
- Real global HAL handles are used; copied-handle ISR mismatch is removed.
- CM200 unlock, direction continuity, signed torque encoding, and rolling counter are implemented.
- CAN hardware errors and FIFO overrun are monitored.
- Shutdown/inverter outputs have one runtime owner and deterministic exception behavior.
- Startup is fail-closed; task/mutex/CAN initialization failures are recorded.
- Safety task heartbeat supervision and vehicle watchdog are implemented.
- BPSD polarity is aligned with the schematic's active-high `BSPD Ok` output.
- ADC operation failures propagate to APPS/BSE/cooling faults.
- APPS raw plausibility is no longer hidden by the torque-demand filter.
- CLI empty-command and SSA return-value defects are fixed.
- MPU6050 signed conversion and power-management register errors are fixed.
- Coolant voltage is no longer mislabeled as temperature.

## Deliberately locked or unresolved

- The BPSD board output is nominally 12 V, while PE13 is 3.3 V and the supplied MCU-breakout schematic shows no level conversion. Hardware interface validation is mandatory.
- Coolant temperature conversion, limits, and closed-loop pump control are not calibrated.
- Vehicle APPS and BSE calibration constants need measured endpoints and tolerances.
- Physical BMS, IMD, TSAL, motor fault/OK, and BPSD polarity/voltage must be validated at the MCU pins.
- The installed CM200 parameter set must be checked for command source, torque scaling, rolling-counter expectations, direction, and enable lockout.
- No ARM target toolchain was available in the revision environment, so the final STM32 ELF/map is not included.
- Host SIL cannot prove interrupt latency, CAN electrical behavior, hardware watchdog timing, output glitches, EMI robustness, or mechanical braking performance.

These unresolved items are why the repository defaults to the output-inhibited bench profile.

