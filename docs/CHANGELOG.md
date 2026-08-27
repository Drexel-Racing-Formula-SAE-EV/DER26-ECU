# Condensed Changelog

This file summarizes the current line without retaining every historical patch note in the repository root.

## v2.10.7 — CAN status/error interrupt hardening

- Adds the dedicated bxCAN `CAN1_SCE` NVIC/vector path.
- Keeps TX/RX0/SCE project configuration synchronized with source and `.ioc` through CI checks.
- Updates runtime/source provenance to v2.10.7.

## v2.10.6 — bench-validation safety/diagnostic release

- Carries the latest complete ECU application tree used for bench validation.
- Includes compile-time inhibited propulsion for bench validation while retaining sensing/CAN/logging/CLI/cooling validation paths.
- Includes hardened CAN TX mailbox ownership and command-state commit handling.
- Includes direct fail-low panic output handling and coolant-pump fail-safe behavior.
- Includes the current DER26-CAN-V4 logger/power-authority integration.

## v2.10.x — DER26-CAN-V4 integration

- Introduced/extended the current AMS power-authority and logging protocol integration.
- Expanded ADBMS/AMS observability carried through the ECU logger/diagnostics.

## Earlier 2.x line

Earlier releases introduced and iteratively hardened static RTOS allocation, current-model authority, SD diagnostics/logging, host CI/stress coverage, and vehicle evidence gates. Those historical one-off release documents were removed from the active root during repository cleanup; Git history remains the canonical detailed history once these updates are committed.
