# ECU / AMS CAN Contract

This ECU firmware supports both the older paged AMS telemetry and the newer compact AMS frames. For new ECU work, use the compact frames first. The older `0x069` stream is kept for bench compatibility and full cell/temp dumps.

## Compact AMS frames used by ECU

All frames are standard 11-bit CAN, DLC 8.

### `0x680` — AMS status and safety gate

| Byte | Meaning |
|---:|---|
| 0 | Protocol version. Current expected version is `1`. |
| 1 | Rolling sequence counter. Should increment every AMS status frame and wrap at 255 -> 0. |
| 2 | AMS state value. ECU stores it but does not reinterpret full AMS internals yet. |
| 3 | Status flags. Bit 0 `BMS_OK`, bit 1 `BMS_INHIBITED`, bit 2 `AMS_HARD_FAULT`, bit 3 `AMS_SOFT_FAULT`, bit 4 `VOLTAGE_VALID`, bit 5 `CURRENT_VALID`, bit 6 `TEMP_VALID`, bit 7 `AMS_CAN_FAULT`. |
| 4 | Fault flags. Bit 0 `VOLTAGE_FAULT`, bit 1 `TEMP_FAULT`, bit 2 `CURRENT_FAULT`, bit 3 reserved/IMD-not-firmware-owned, bit 4 `CHARGER_FAULT`, bit 5 `ADBMS_DIAG_FAULT`, bit 6 `TASK_HEARTBEAT_FAULT`, bit 7 `LOGGER_HEARTBEAT_FAULT`. |
| 5 | Voltage fault reason. |
| 6 | Temperature fault reason. |
| 7 | Current fault reason. |

ECU torque must be blocked when `0x680` is stale, repeated with the same sequence, `BMS_OK` is false, any validity bit is false, or any relevant fault bit is set. A status frame does not make the whole AMS snapshot fresh: the ECU tracks `0x680`, `0x681`, and `0x682` timestamps independently.

Do **not** treat byte 4 bit 3 as firmware-validated IMD health yet. For this staged bench firmware, IMD remains handled outside this compact AMS parser.

### `0x681` — Electrical summary

| Bytes | Meaning |
|---:|---|
| 0-1 | Pack voltage, unsigned, 0.1 V/count. |
| 2-3 | Pack current, signed, 0.1 A/count. |
| 4-5 | Minimum cell voltage, mV. |
| 6-7 | Maximum cell voltage, mV. |

The ECU treats these as plausible only when cells are within 500-5000 mV, minimum is no greater than maximum, pack voltage is no greater than 1000.0 V, pack current is within -1000.0 to +1000.0 A, and the pack voltage is consistent with a 75-series pack bounded by the transmitted minimum and maximum cell values. A 200 mV allowance covers the 0.1 V pack encoding and source rounding. These are corruption/data-integrity bounds, not substitutes for AMS trip thresholds.

### `0x682` — Thermal summary

| Bytes | Meaning |
|---:|---|
| 0-1 | Maximum temperature, signed, 0.1 C/count. |
| 2-3 | Minimum temperature, signed, 0.1 C/count. |
| 4-5 | Average/filtered temperature, signed, 0.1 C/count. |
| 6 | Maximum fan command percent. |
| 7 | Thermal/fan flags. |

The ECU requires all three temperatures within -40.0 to 150.0 C, `minimum <= average <= maximum`, and fan command 0-100%.

Byte 7 is decoded as follows:

| Bit | Meaning | Motoring torque action |
|---:|---|---|
| 0 | temperature warning | diagnostic/derate candidate |
| 1 | fan maximum command | diagnostic |
| 2 | charge stop | does not alone block motoring |
| 3 | overtemperature pending | diagnostic/derate candidate |
| 4 | overtemperature fault | block |
| 5 | severe overtemperature fault | block |
| 6 | fan fault | diagnostic; AMS status fault policy remains authoritative |
| 7 | temperature invalid/read fault | block |

### `0x683` — Location and count summary

| Byte | Meaning |
|---:|---|
| 0 | Segment containing maximum cell voltage. |
| 1 | Cell index containing maximum cell voltage. |
| 2 | Segment containing minimum cell voltage. |
| 3 | Cell index containing minimum cell voltage. |
| 4 | Segment containing maximum temperature. |
| 5 | Sensor index containing maximum temperature. |
| 6 | Usable cell count. |
| 7 | Usable temperature-sensor count. |

Location/count fields are range-checked against five segments, 15 cells per segment, 24 temperature channels per segment, 75 total cells, and 120 total temperature channels. This health frame remains diagnostic rather than a required torque-authority heartbeat.

## Legacy frames kept for bench compatibility

`0x069` is the older paged AMS telemetry. It carries state, AIR state, current, IMD legacy fields, min/max voltage, per-segment cell voltages, per-segment temperatures, and fans over packet headers 0-71.

`0x421` is the older estimator/status frame. The ECU stores its four raw 16-bit words only.

## ECU-side torque gate

The ECU sets `ams_fault` from `ams_allows_torque()`. With compact status available, torque is allowed only when:

- AMS status is fresh.
- Electrical and thermal summaries have both been received and are independently fresh (500 ms maximum age).
- `BMS_OK` is true.
- AMS inhibit is false.
- Voltage/current/temp validity bits are true.
- AMS hard/soft fault bits are false.
- Voltage/current/temp fault bits are false.
- Charger, ADBMS diagnostic, task heartbeat, logger heartbeat, and AMS CAN fault bits are false.
- The compact protocol version matches the ECU-supported version.
- The status sequence is coherent. Repeated/stuck frames and sequence jumps block torque until a coherent next status frame is received.
- Electrical `min_cell <= max_cell`, the pack voltage lies within the 75-series min/max-cell bounds (plus 200 mV encoding tolerance), thermal `min_temp <= max_temp`, and fan command is 0-100%.
- Electrical/thermal physical-plausibility envelopes and thermal average ordering pass.
- Thermal bits 4, 5, and 7 are clear.

Wrong DLC, non-standard format, null/missing data, or a remote frame for a required compact ID immediately invalidates that frame's last good state. The ECU does not continue trusting an earlier payload until its former 500 ms timeout.

Legacy `0x069` frames are still decoded for bench visibility and old logs, but they are no longer sufficient to allow torque. ECU torque gating requires the compact `0x680` status frame.

## Authoritative dynamic power protocol v2 (`0x684`-`0x68A`)

The compact `0x680`-`0x683` frames remain the AMS health gate. Dynamic current/power authority is separately carried by the protocol-v2 bundle:

- `0x684`: active DCL and discharge power limit.
- `0x685`: active charge/regen CCL and charge power limit.
- `0x686`: SoH.
- `0x687`: three constant-current feasibility horizons: `0.1 s`, `10 s`, `30 s`.
- `0x689`: optional synchronized strategy/resource state.
- `0x68A`: optional synchronized per-horizon binding metadata.

`0x684`-`0x687` are staged atomically with ID-bound CRC, a common modulo-16 counter, 50 ms maximum bundle skew, 250 ms maximum age, and a two-consecutive-good-bundle recovery gate. Any malformed required frame revokes scalar authority. Optional advisory corruption only invalidates that advisory channel.

The APPS task converts candidate torque into conservative steady and transition pack-current intervals using the source-independent numeric clamp. The final CM200 transmit task waits for a hardware mailbox, then performs the newest coherent authority/capability re-read and bxCAN enqueue within one CAN-RX-masked commit section. The final check compares cached intervals against DCL, CCL, and direction authorization and performs zero model calls/searches. A stale, fallback, direction-inhibited, numerically tightened, capability-changed, or zero authority produces a disable command. `ECU_AMS_POWER_CLAMP_IMPLEMENTED=1`; vehicle builds remain locked by independent validation evidence and the deliberately invalid checked-in calibration.
