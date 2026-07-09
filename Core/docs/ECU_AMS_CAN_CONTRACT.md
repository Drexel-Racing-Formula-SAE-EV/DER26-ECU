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

ECU torque must be blocked when `0x680` is stale, repeated with the same sequence, `BMS_OK` is false, any validity bit is false, or any relevant fault bit is set.

Do **not** treat byte 4 bit 3 as firmware-validated IMD health yet. For this staged bench firmware, IMD remains handled outside this compact AMS parser.

### `0x681` — Electrical summary

| Bytes | Meaning |
|---:|---|
| 0-1 | Pack voltage, unsigned, 0.1 V/count. |
| 2-3 | Pack current, signed, 0.1 A/count. |
| 4-5 | Minimum cell voltage, mV. |
| 6-7 | Maximum cell voltage, mV. |

### `0x682` — Thermal summary

| Bytes | Meaning |
|---:|---|
| 0-1 | Maximum temperature, signed, 0.1 C/count. |
| 2-3 | Minimum temperature, signed, 0.1 C/count. |
| 4-5 | Average/filtered temperature, signed, 0.1 C/count. |
| 6 | Maximum fan command percent. |
| 7 | Thermal/fan flags. |

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

## Legacy frames kept for bench compatibility

`0x069` is the older paged AMS telemetry. It carries state, AIR state, current, IMD legacy fields, min/max voltage, per-segment cell voltages, per-segment temperatures, and fans over packet headers 0-61.

`0x421` is the older estimator/status frame. The ECU stores its four raw 16-bit words only.

## ECU-side torque gate

The ECU sets `ams_fault` from `ams_allows_torque()`. With compact status available, torque is allowed only when:

- AMS status is fresh.
- `BMS_OK` is true.
- AMS inhibit is false.
- Voltage/current/temp validity bits are true.
- AMS hard/soft fault bits are false.
- Voltage/current/temp fault bits are false.
- Charger, ADBMS diagnostic, task heartbeat, logger heartbeat, and AMS CAN fault bits are false.
- The status sequence is not repeated.

If compact status has not been seen yet, ECU falls back to the legacy stale check for old bench firmware only.
