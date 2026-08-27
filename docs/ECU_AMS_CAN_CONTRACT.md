# DER26 ECU ↔ AMS CAN Contract — V4

Current ECU source: v2.10.7. The interface remains the DER26-CAN-V4 contract; verify the paired AMS build advertises the matching protocol before vehicle testing.

DER26-CAN-V4 separates battery authority from observational telemetry. The ECU
may only grant battery-dependent torque when the compact AMS health contract
and required dynamic power bundle are fresh, coherent, and valid. Passive
logger/detail frames are recorded for diagnosis and can never create torque
authority.

## Bus contract

- Classical CAN 2.0.
- Vehicle target: **500 kbit/s**.
- Safety-correct validation/fallback mode: 250 kbit/s; detail service may
  degrade while protected deadlines must remain correct.
- STM32F767 CAN clock: 54 MHz from HSE/PLL.
- 18 TQ/bit, BS1=15, BS2=2, SJW=2.
- Prescaler 6 @500k, 12 @250k.
- Standard 11-bit IDs for DER compact/detail traffic; third-party extended IDs
  retain their documented format.

## Protected AMS health frames

| ID | Meaning |
|---:|---|
| `0x680` | compact status, protocol/sequence, BMS/inhibit/validity flags |
| `0x681` | pack voltage/current and min/max cell |
| `0x682` | min/max/average temperature and fan command |
| `0x683` | cell/temp locations and usable counts |

The ECU validates format/DLC/RTR, protocol version, sequence progression,
physical plausibility, measurement validity, and freshness. Required compact
data stale or malformed => battery-dependent torque authority is false.

## Dynamic power authority protocol v2

| ID | Requirement | Meaning |
|---:|---|---|
| `0x684` | required | DCL / discharge power authority |
| `0x685` | required | CCL / charge-regeneration authority |
| `0x686` | required | SoH/resource data |
| `0x687` | required | feasibility envelope |
| `0x688` | ECU→AMS feature-gated control | mission request |
| `0x689` | advisory | strategy/resource state |
| `0x68A` | advisory | binding metadata |
| `0x68B` | advisory | current-source diagnostics |

`0x684..0x687` retain ID-bound CRC, common modulo-16 counter, 50-ms maximum
bundle skew, 250-ms maximum authority age, and two-consecutive-good-bundle
recovery qualification. Optional advisory corruption/absence cannot revoke an
otherwise valid scalar bundle.

The ECU torque clamp consumes only trusted compact/power authority. Final
ECU→CM200 positive-torque commit remains synchronous: the task uses a bounded
mailbox-opportunity wait, re-reads the newest AMS authority immediately before
commit, and never permits an unbounded stale positive-torque queue. Expiry/CAN
unavailability is fail-zero/fail-disabled and marks CM200 TX unhealthy.

## Passive V4 AMS detail stream

The passive logger/detail range is `0x690..0x6C0`:

- `0x690..0x69F`: system/fault/CAN/watchdog/link summaries
- `0x6A0`: cells, 3 per frame
- `0x6A1`: temperatures, 3 per frame
- `0x6A2..0x6AC`: masks and diagnostic detail
- `0x6AD`: coherent detail snapshot metadata
- `0x6AE..0x6B0`: ADBMS2950/APM data when enabled
- `0x6B1`: relocated legacy paged compatibility ID; vehicle V4 transmission is
  disabled by default
- `0x6B2`: estimator diagnostics, moved from historical `0x421`
- `0x6B3`: V4 AMS TX-scheduler/recovery diagnostics
- `0x6B4`: passive estimator RAW/AVG8/IIR voltage-product comparison
- `0x6B5..0x6C0`: five-segment DADEKF, SoP and fuse tuning records

Historical pre-V4 `0x069` and `0x421` may remain recognized by offline decoding
for old logs, but are not accepted as production vehicle V4 authority and are
not required by the live V4 filters.


### `0x6B4` estimator voltage-product comparison

`0x6B4` is passive experiment/diagnostic data only. Byte 0 identifies the active estimator; byte 1 carries RAW/AVG8/IIR validity and selected live estimator source; bytes 2-3 are signed AVG8-minus-RAW mV; bytes 4-5 are signed IIR-minus-RAW mV; bytes 6-7 carry the low 16 bits of the source measurement sequence. The ECU may log/decode this frame but it must not use it in `ams_allows_torque()` or any battery-authority decision.

The same authority exclusion applies to `0x6B5..0x6C0`. ECU accepts these IDs
only for the bounded raw SD ring and offline decoder; no live AMS torque or
safety structure consumes their payloads.

### Snapshot coherence and gap accounting

`0x6AD` version 2 carries snapshot sequence, phase, phase count, and source
measurement sequence. Cell/temperature fragment tags include a 5-bit snapshot
sequence plus 3-bit phase/segment tag. ECU tracks:

- snapshot sequence gaps;
- incomplete snapshots;
- missing phases;
- missing cell/temperature fragments;
- duplicate fragments;
- out-of-order fragments.

These are logged for post-event interpretation. Detail loss has no torque-
authority consequence.

### `0x6B3` source TX-scheduler diagnostics

The ECU decodes source-side protected deadline misses, detail supersessions,
recovery discards, protected supersessions, TX suspend/latch state, bus-off, and
recovery-pending flags. This lets an SD log distinguish source-side intentional
detail degradation from ECU-side raw/logger loss.

## ECU→AMS diagnostic feedback — `0x6F0`

The ECU emits an observability-only standard data frame at 2 Hz:

| Byte(s) | Meaning |
|---:|---|
| 0 | feedback protocol version |
| 1 | last AMS protected sequence accepted |
| 2 | last AMS detail snapshot sequence observed |
| 3 | AMS fresh / authority valid / detail complete / raw-log-overflow flags |
| 4-5 | ECU RX/drop diagnostic summary |
| 6-7 | uptime/counter |

The AMS uses this only for diagnostics. It must never make BMS_OK, AIR,
balancing, charger, or battery-authority decisions from the feedback frame.

## ECU CAN RX behavior

The ECU owns CAN1 filter banks `0..13`; CAN2 begins at bank 14. Exact CM200
filters are installed first and a standard-data mask accepts `0x680..0x6FF` for
AMS V4 traffic. Remote frames are rejected in software even when a hardware
filter should already exclude them. RX ISR remains bounded: copy/header checks
and raw enqueue only; protocol decode and state publication remain task-context
work.

The FIFO is intentionally latest-biased rather than locked. Any hardware FIFO
overrun or software/raw-logger drop is separately counted so post-run analysis
can distinguish transport loss from application/source supersession.

## Arbitration invariant

For DER-owned frames, lower CAN-ID numerical value must mirror higher software
traffic priority. Compact/protected AMS traffic therefore precedes passive V4
detail. Historical `0x069` was retired specifically because its low numerical ID
let observational traffic outrank battery authority with bxCAN TXFP disabled.

Fixed CM200/charger identifiers are external constraints and are included in the
whole-vehicle response-time/load analysis rather than renumbered by DER code.

## Current-source diagnostic `0x68B`

The frame records source, quality, current-valid physical boundary, source epoch,
sample sequence and sample age. The ECU logs it but does not use source identity
to grant torque; validity remains owned by compact status and current/power
limits remain owned by the protected protocol.

## Staleness and fail-zero behavior

Loss of required compact or power authority causes battery-dependent torque
permission to become false. Detail can be stale, incomplete, or entirely absent
without creating or revoking authority. This separation is intentional: under
congestion, diagnostic temporal density degrades before safety/control behavior.

## Vehicle qualification boundary

Host/SIL checks V4 ID/clock contracts, filters, decoding, gap accounting,
CM200 bounded-commit behavior and logging. Final vehicle qualification still
requires target build/flash, physical 500-kbit/s testing, exact charger/CM200
configuration, CANalyzer load/response-time measurement, full expected-node
traffic and fault injection.
