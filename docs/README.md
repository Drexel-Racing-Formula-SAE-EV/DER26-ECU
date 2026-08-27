# ECU Documentation

This directory is the maintained documentation set for the DER26 ECU firmware.

## Start here

- [Architecture](ARCHITECTURE.md) — responsibility boundaries and data/control flow.
- [Safety model](SAFETY_MODEL.md) — authority, fail-low behavior, and evidence gates.
- [Code organization](CODE_ORGANIZATION.md) — map of project-authored source modules.
- [Validation strategy](VALIDATION.md) — host, CI, target, HIL, and vehicle validation layers.
- [Current source status](STATUS.md) — current version and open qualification work.
- [Repository layout](REPOSITORY_LAYOUT.md) — what belongs where and what is intentionally excluded.
- [Changelog](CHANGELOG.md) — condensed current release history.
- [Attribution](ATTRIBUTION.md) — project/vendor ownership boundaries.

## Interfaces and control contracts

- [ECU ↔ AMS CAN contract](ECU_AMS_CAN_CONTRACT.md)
- [AMS power protocol v2](ECU_AMS_POWER_PROTOCOL_V2.md)
- [CM200 CAN contract and bring-up](CM200_CAN_CONTRACT_AND_BRINGUP.md)
- [Torque-to-pack-current clamp](ECU_TORQUE_TO_PACK_CURRENT_CLAMP.md)
- [Torque removal and availability budget](ECU_TORQUE_REMOVAL_AND_AVAILABILITY_BUDGET.md)
- [Current-model certification campaign](ECU_CURRENT_MODEL_CERTIFICATION_CAMPAIGN.md)

## Hardware / service / logging

- [ECU hardware bring-up](ECU_HARDWARE_BRINGUP.md)
- [Pin map](PIN_MAP.md)
- [BSPD interface and test plan](BSPD_INTERFACE_AND_TEST_PLAN.md)
- [SD/CAN data logger](ECU_SD_CAN_DATA_LOGGER.md)
