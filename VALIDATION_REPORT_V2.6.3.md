# DER26 ECU Firmware v2.6.3 Validation Report

## Completed in this environment

- GCC host CI with `-Wall -Wextra -Werror`
- Revision 7 invariant probe
- Power-bundle availability budget probe
- Current residual monitor tests
- APPS/BSE elapsed-time boundary tests
- Python/C current-model differential vectors
- ECU unit and regression suites
- AMS protocol consumer, integration, and golden-vector tests
- ECU system SIL/fault-injection suite
- Vehicle-profile release-gate checks
- Static-allocation gate
- Residual-authority gate
- Current-model evidence/CRC gate
- GCC host analyzer
- Clang analyzer
- Full 39-file application host syntax check for bench and fully acknowledged
  vehicle profiles
- Full 39-file bench application GCC analyzer
- AddressSanitizer and UndefinedBehaviorSanitizer suites
- Repository structure and hygiene checks

## Measured contract results

- Revision 7 invariant probe: PASS
- Worst observed clamp probe work: 4 torque cells, 4 steady calls, 2 transition calls
- One dropped 10 Hz power bundle: 110 ms zero-authority outage
- Two consecutive dropped bundles: 150 ms zero-authority outage

## Not completed

The ARM target toolchain and physical hardware were unavailable, so this report
does not claim:

- Final STM32 ELF/map generation
- Target flash/RAM figures
- Task stack high-water measurements
- DWT WCET/jitter measurements
- CAN ISR or critical-section timing
- Physical CAN arbitration/bus-load timing
- HIL, dyno, lifted-wheel, or HV validation
