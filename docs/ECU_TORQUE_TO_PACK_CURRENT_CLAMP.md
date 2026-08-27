# DER26 ECU Torque-to-Pack-Current Clamp

## Status

The deterministic 100 Hz software path is implemented in v2.6.1. The checked-in calibration remains deliberately unqualified, and vehicle use remains locked behind `ECU_AMS_POWER_CLAMP_VALIDATED` plus the BSPD and CM200 evidence gates.

Implementation complete does not mean calibrated or vehicle certified.

## Safety contract

- Positive pack current means accumulator discharge.
- The AMS owns DCL, CCL, and current-direction authorization.
- The ECU model predicts the canonical battery-current boundary and does not branch on DHAB versus APM.
- Zero traction torque is always commandable and is exempt from direction authorization.
- Current intervals are compared directly with zero, DCL, and `-CCL`. Numerical error widens the interval outward; it is not a physical current deadband.
- Raw torque is used for steady prediction, active-span extrema, and transition lookup. Zero-band normalization affects classification only.
- A commanded zero does not prove physical zero. Last nonzero sign survives the zero dwell, so `+T -> 0 -> -T` remains reversal-gated.
- Optional increases must pass every whole torque cell along the traversed path, an exact final steady check, and the applicable transition envelope.
- Reductions search farther toward zero and never retain a held torque that is no longer feasible.
- Transition state persists across cycles and is indexed by the full raw commanded bracket, not the latest 10 ms delta.
- Small same-sign settled microsteps may use a separately certified margin only while step, command-rate, anchor-deviation, and cumulative-drift limits all pass.
- The final protected commit performs zero model calls and zero searches. Any new authority, capability, calibration, or operating-point inconsistency produces zero.
- Clamp state changes only after the command is accepted into the CM200 hardware transmit mailbox.

## Runtime split

### APPS task: bounded model/search producer

The APPS task:

1. forms the comfort-slew candidate;
2. snapshots CM200 operating point/capability and coherent AMS authority;
3. runs steady and transition model/search logic;
4. publishes a `ecu_torque_command_contract_t` containing the selected torque, cached current intervals, model-call counts, generations, and fallback-zero envelope.

It does not claim the command was sent and does not mutate committed transition/sign state.

### CAN task: actual protected commit

After waiting for a bxCAN mailbox, the CAN task:

1. masks concurrent CAN/task updates for the short commit section;
2. reads the newest coherent AMS authority and CM200 capabilities/generations;
3. compares the cached final steady/transition intervals only;
4. converts any invalidated nonzero command to zero;
5. calls `HAL_CAN_AddTxMessage()`;
6. updates committed torque, sign/path/transition/tracking state, and residual-monitor prediction only after HAL accepts the packet.

A queued command cannot therefore update state before hardware-mailbox acceptance, and late DCL/CCL tightening cannot bypass the numerical clamp.

## Calibration schema v2

The boot loader accepts at most:

```text
21 torque-axis points
20 whole torque cells
4 steady operating regions per cell
64 transition cells
4 transition operating regions per transition cell
```

The runtime search limits are compile-time coupled to these maxima. Oversized or malformed artifacts are rejected during boot qualification.

### Steady cells

Each steady cell stores a constant pack-current interval valid over the complete torque extent and a declared speed/Vdc/temperature region. Direct cell-index evaluation is used during path search; boundary point lookup is not used as proof of traversed-cell feasibility.

Exact final evaluation constructs uncertainty intervals for:

- torque;
- speed, including asymmetric sensor/age/acceleration and deceleration/regrip terms;
- Vdc;
- inverter temperature.

Every touched torque cell and operating region is unioned. Region-cap overflow discards the partial result and fails closed.

### Transition cells

Each transition cell is indexed by:

```text
physical command profile
transition direction
maximum full active raw span
speed region
Vdc region
temperature region
```

Entries contain an absolute canonical pack-current interval and maximum settling time. Calibration validation requires larger span entries for the same profile/direction/domain to contain the smaller-span current interval and use no shorter settling time.

Schema v2 supports:

- slew-limited increases/decreases;
- composed command sequences;
- zero assertion;
- reversal first leg;
- microstep metadata and separately certified tracking margins.

## Boot-only qualification

Full schema checks and CRC32 run during `ecu_pack_current_calibration_qualify()`. Runtime control receives an immutable qualified handle containing the approved pointer, CRC, generation, and qualification flag.

The 100 Hz model path performs only bounded handle checks. It does not recalculate the calibration CRC during search or final commit.

The checked-in artifact intentionally contains no steady or transition cells, has `evidence_valid=false`, and has no release CRC. It cannot qualify or grant nonzero torque.

## Whole-cell search

### Increase

The clamp enumerates cell indices that overlap the raw path from committed torque to request. It evaluates each complete touched cell in path order and stops at the first unproven/infeasible cell. The selected torque may only lie in the contiguous certified prefix.

### Reduction

If the requested reduction is not feasible, the clamp examines cells farther toward zero. A nonzero result is returned only after exact verification. Search exhaustion commands zero.

### Transition refinement

A steady-feasible optional increase that fails transition authority is reduced by a fixed four-iteration refinement. The result must pass exact steady and transition checks. Otherwise the clamp holds only a still-feasible current torque or reduces to zero.

## Settled tracking and re-anchoring

Persistent state contains:

- settled raw torque anchor;
- active minimum/maximum raw command;
- latest target;
- cumulative raw drift;
- last material-change and hardware-commit timestamps;
- profile/direction;
- maximum settling time;
- required/observed steady confirmation samples.

Sign/zero/reversal classification precedes tracking eligibility. A tracking band never authorizes a sign or zero crossing.

An active transition ends only after the larger of calibrated settling time and settled-tracking time has elapsed, command rate remains under the calibrated threshold, the command remains within the tracking band, and the required consecutive samples pass. Re-anchoring then resets active span and cumulative drift.

## Auxiliary current

One healthy R2D interval is applied exactly once:

```text
Iaux,min = 0 A
Iaux,max = 0.250 A provisional
```

The lower bound gives no credit against regeneration. The upper bound remains a commissioning placeholder pending the final DHAB/APM branch-return drawing and small HV sensing-load inventory.

Precharge, charging, discharge, and TS-off states prohibit propulsion torque through independent vehicle-state gating and do not require a runtime auxiliary-mode state machine.

## Battery-authority states

The clamp reports:

- `NORMAL`;
- `LOW` when calibrated headroom/utilization thresholds are crossed;
- `TORQUE_EXHAUSTED` when no verified nonzero torque remains;
- `ZERO_STEADY_AUX_INFEASIBLE` when the certified zero-torque steady interval itself exceeds DCL/CCL.

Zero remains the command in every state.

## Execution bounds

The source defines and tests conservative maximum counts:

```text
steady model calls        <= 32
transition model calls    <= 8
torque-cell evaluations   <= 64
transition refinements    <= 4
commit-path model calls   = 0
```

A defensive count violation converts the result to the pre-certified static zero/decay envelope. These are software bounds, not target WCET proof; cycle and stack measurements on STM32F767 remain release requirements.

## Residual monitor

The monitor compares time-aligned canonical measured pack current with the interval published for the torque actually accepted by bxCAN. Phases are:

- step transition;
- slew tracking;
- settling;
- steady.

Stale or invalid measurements count as violations. A source-epoch change resets persistence and enters bounded source settling without clearing a latched fault. Since auxiliary current is not independently measured, attribution remains `TOTAL_ENVELOPE_VIOLATION`.

The current AMS electrical frame does not include the physical acquisition timestamp. Runtime therefore uses coherent frame receive time, and certification must include AMS acquisition/filter/publication latency plus CAN/receive jitter in the alignment bound. Automatic mid-run current-source failover remains disabled, so the first-release source epoch is established at boot and does not change during normal operation.

The checked-in persistence/age/alignment values are provisional and must be replaced by certification evidence. The CLI `power` command exposes the clamp, transition, prediction, residual, source-epoch, calibration, and deadline-overrun diagnostics needed during that campaign.

## Release blockers

- certified steady whole-cell bounds;
- low-speed/stall and four-quadrant evidence;
- certified transition/composed-sequence domination envelopes;
- certified microstep margin and settling criteria;
- final auxiliary and canonical current-boundary evidence;
- target WCET, stack, ISR interference, and deadline behavior;
- CM200 command timeout/EEPROM validation;
- HIL, dyno, holdout, and restricted vehicle tests.
