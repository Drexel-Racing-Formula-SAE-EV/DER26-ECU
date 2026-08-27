# Repository Layout

This is a standalone ECU firmware repository, not a vehicle monorepo.

```text
.
├── Core/                  project application/generated STM32 source
├── Drivers/               STM32 HAL/CMSIS vendor code
├── Middlewares/           FreeRTOS/vendor middleware
├── FATFS/                 FatFs integration
├── host_tests/            host verification harness
├── ci/                    source-contract and target-build CI
├── Tools/                 offline analysis utilities
├── docs/                  maintained engineering documentation
├── .github/workflows/     GitHub Actions CI
├── DER26-ECU.ioc          CubeMX project configuration
├── .project/.cproject     STM32CubeIDE project metadata
└── STM32F767ZITX_*.ld     target linker scripts
```

## Kept intentionally

- CubeIDE/CubeMX metadata required to reproduce the target project.
- Vendor middleware and generated support needed for the firmware build.
- Host tests and CI scripts because they are part of the software safety/maintenance case.
- Operational hardware/CAN/control documentation that matches current source.

## Excluded intentionally

- `Debug/` and other generated build directories;
- ELF/HEX/MAP/object/dependency/listing files;
- local IDE `.settings`, workspace metadata, and launch files;
- obsolete patch/review/validation snapshots superseded by maintained docs;
- temporary host-test build products;
- credentials/secrets.
