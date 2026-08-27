# ECU SD/CAN Data Logger

## Purpose

The ECU best-effort, non-safety data recorder introduced in v2.8.0 and hardened during the DER26-CAN-V4 work is retained in the current ECU v2.10.7 source. It records both decoded ECU/AMS/CM200 state and every CAN frame accepted by the ECU hardware filter while a logging session is active.

The logger is deliberately separated from control authority:

- It runs at priority 2, below every ECU control task.
- It owns no torque, RTD, BSPD, AMS, CM200, or shutdown state.
- CAN reception never performs an SD/FatFs operation.
- The ISR writes into a fixed single-producer/single-consumer ring and drops records if the ring is full.
- A logger failure increments diagnostics but does not relax a safety gate or change an output.
- All memory is statically allocated.

## Session files

Each new session uses the first free index from `000` through `999` and creates four files without overwriting an existing session:

| File | Contents |
|---|---|
| `ECU###.CSV` | Decoded 1-100 Hz ECU snapshot; default 10 Hz. |
| `CAN###.BIN` | Raw CAN records captured at receive time. |
| `EVT###.CSV` | Start, stop, operator marker, and raw-ring drop events. |
| `META###.TXT` | Firmware/profile/schema and matching file names. |

The bench build attempts to start a session automatically after startup and retries every five seconds when no usable card is available. Manual `log start`, `log stop`, or `log new` disables that runtime auto-start behavior until the next reset.

## Decoded CSV coverage

`ECU###.CSV` includes:

- APPS and brake percentages, RTD state, ECU hard/soft/startup faults, firmware and motor-enable state.
- Brake light and coolant pressure, flow, inlet/outlet temperature, and coolant-valid status.
- CAN accepted/ignored/malformed/remote/overrun/error counters.
- Target and commanded torque, clamp reason/validity, battery-authority state, and current-model residual diagnostics.
- AMS compact `0x680-0x683` status, electrical, thermal, health, freshness, sequence, fault, and validity data.
- AMS `0x68B` current-source/quality/age diagnostics.
- Passive AMS logger stream sequence, snapshot phase, source measurement sequence, receive count, and age.
- ADBMS2950 current channels, voltage channels, health flags, stage, and reason from `0x6AE-0x6B0`, V4 scheduler/voltage-product diagnostics at `0x6B3-0x6B4`, and passive DADEKF/SoP/fuse tuning at `0x6B5-0x6C0`.
- AMS dynamic DCL/CCL and discharge/charge power authority.
- CM200 readiness/fault, speed, DC voltage/current, commanded/feedback torque, temperatures, state, and fault words.

Column names include the stored engineering scale, such as `_0p1V`, `_0p1A`, `_mV`, or `_0p1C`.

## Raw CAN format

`CAN###.BIN` begins with a 32-byte header:

- Bytes `0-6`: ASCII `DERCAN1`.
- Byte `8`: raw schema version.
- Bytes `10-11`: little-endian record size; currently 24 bytes.
- Bytes `12-14`: ECU firmware major/minor/patch.

Each packed 24-byte little-endian record contains:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | `timestamp_ms` |
| 4 | 4 | monotonically increasing capture sequence |
| 8 | 4 | CAN identifier |
| 12 | 1 | DLC, clamped to 8 |
| 13 | 1 | flags |
| 14 | 8 | payload bytes |
| 22 | 2 | reserved |

Flag bits are:

- bit 0: standard identifier
- bit 1: remote frame
- bit 2: known AMS identifier
- bit 3: known CM200 identifier
- bit 4: production parser accepted the payload

Decode a raw file on a computer with:

```bash
python Tools/decode_can_log.py CAN000.BIN
```

The generated CSV includes identifier names for the compact AMS, dynamic power, current-source, and `0x690-0x6C0` passive V4 telemetry streams, relocated estimator/compatibility IDs `0x6B1/0x6B2`, and ECU→AMS feedback `0x6F0`; historical pre-V4 `0x069/0x421` names remain available for old-log decoding. Derived tables include separate EKF, covariance, SoH, SoP, and fuse CSVs while `CAN###.BIN` remains the source of truth.

## Runtime CLI

```text
log status
log start
log stop
log flush
log new
log rate 10
log raw on
log raw off
log mark 1
```

`log status` reports active/files-open state, session index, decoded/raw/event counts, ring usage and high-water mark, dropped raw records, sync count, write errors, and the last FatFs result.

`log flush` drains the raw ring and synchronizes all open data files. `log stop` first disables the ISR producer, drains pending raw records, writes a STOP event, synchronizes, and closes the files. Do not remove the card until the logger is stopped and `sdcard unmount` has completed.

SD destructive tests and unmount are refused while the logger owns open files.

## Filesystem ownership and power-loss behavior

The SD service uses one statically allocated recursive mutex for all FatFs operations. The logger synchronizes decoded, raw, and event files every second. A sudden power loss can therefore lose or corrupt the most recent unsynchronized interval, but cannot be made completely power-fail-safe without a journaling filesystem or energy hold-up.

For intentional shutdown:

```text
log flush
log stop
sdcard unmount
```

Wait for `active:0 files:0` before removing power or the card.

## CAN receive coverage

The existing exact safety/control filters remain in place. An additional 32-bit standard-data mask accepts `0x680-0x6FF`, covering compact AMS, dynamic power, current-source diagnostics, and passive logger frames. Remote and extended frames are rejected by the range-filter mask. Unknown standard data frames outside configured filters never enter the ECU receive ISR.

Passive logger frames can populate observability fields and raw files, but they never participate in `ams_allows_torque()`. Torque authority remains based on the compact safety gate and dynamic power protocol.

## Bench acceptance test

1. Insert the validated FAT32 card with power off.
2. Flash mutually compatible DER26-CAN-V4 ECU/AMS images and verify the runtime build/protocol identifiers before testing. This repository provides ECU v2.10.7.
3. Confirm approximately 60 ohms between CANH and CANL on the complete two-node bus before power-up.
4. Power both controllers and run:

```text
log status
can
ams
```

5. Confirm `active:1`, `files:1`, increasing decoded/raw counts, zero drops, and zero write errors.
6. Add an event marker, run the CAN test, then close cleanly:

```text
log mark 1
log flush
log stop
sdcard unmount
```

7. On a computer, verify all four matching session files and decode `CAN###.BIN`.

A strong pass has increasing AMS compact/logger counts, changing AMS sequence values, no CAN bus-off, no logger drops, no SD write errors, and a clean STOP event.

## Offline AMS extraction

The decoder preserves every received frame in the main CSV and also creates four
long-form analysis files by default:

```bash
python Tools/decode_can_log.py CAN000.BIN
```

- `CAN000_ams_cells.csv`: one row per cell sample with logger sequence, phase,
  measurement sequence, segment, zero-based index, one-based cell number, mV,
  and validity.
- `CAN000_ams_temps.csv`: one row per thermistor sample with snapshot context,
  sensor index, 0.1 C value, and validity.
- `CAN000_ams_snapshots.csv`: passive logger protocol/sequence/phase metadata.
- `CAN000_ams_apm.csv`: ADBMS2950 currents, voltages, health, conversion count,
  raw codes, conversion phase, and selected calibration profile.

The main `CAN000.csv` also includes `decoded_fields_json` for high-value compact,
cell, temperature, mask, snapshot, and APM frames. Use `--no-derived` when only
the one-row-per-frame CSV is needed.
