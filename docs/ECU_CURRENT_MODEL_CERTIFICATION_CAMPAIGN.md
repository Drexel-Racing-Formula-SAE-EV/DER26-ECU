# ECU Current-Model Certification Campaign

## Purpose

This campaign converts the implemented v2.6.1 safety contract into measured calibration evidence. Passing software tests alone does not authorize vehicle torque.

The campaign shall generate independent artifacts for:

- steady whole-cell current bounds;
- transition absolute-current envelopes;
- settled-tracking/microstep margins;
- healthy R2D auxiliary current;
- degraded fallback, if a nonzero degraded cap is ever desired;
- residual-monitor timing and persistence;
- STM32F767 WCET/stack/deadline evidence.

## Required equipment and timing

Use:

- a traceable pack-current reference with accuracy materially better than the intended bound;
- canonical DHAB and/or APM current logging;
- synchronized ECU torque request, selected torque, hardware-mailbox commit timestamp, CM200 torque/current/speed/Vdc/temperature telemetry, and AMS DCL/CCL;
- a common monotonic time base or measured clock-offset/drift correction;
- environmental temperature measurement;
- a repeatable load/dyno or HIL setup capable of positive and negative torque.

Document:

- sensor accuracy, quantization, noise, thermal drift, and filter delay;
- CAN and logger timestamp semantics;
- CM200 command-processing and torque-response delay;
- command cadence/jitter;
- DHAB/APM source and source epoch;
- all uncertainty added after measurement.

## Data governance

Partition data before fitting:

```text
training
margin selection
independent holdout
```

Holdout data shall not be used to alter coefficients, cell bounds, transition envelopes, microstep margins, settling times, or residual thresholds. Any change after holdout inspection creates a new calibration candidate and a new holdout campaign.

Every exported artifact shall identify:

- hardware revision;
- software revision;
- CM200 firmware/EEPROM configuration;
- accumulator/motor/inverter configuration;
- test date and equipment calibration status;
- source data hashes;
- fitting/export tool revision;
- evidence state and CRC.

## Canonical current boundary and auxiliary closure

Before model fitting:

1. update the tractive schematic with the APM shunt, DHAB aperture, positive-current arrows, AIR−, and every branch return;
2. prove which current returns traverse each sensor;
3. classify charger, precharge, discharge, HV-box, RTM, indicator, energy-meter, and IMD branches;
4. establish DHAB/APM boundary equivalence or reject interchangeability;
5. measure or bound every healthy R2D auxiliary branch crossing the canonical sensor.

The provisional `[0, 0.250 A]` auxiliary interval may be retained only when the inventory and measurements support it. A branch fault is outside the healthy-domain constant and is handled by residual detection plus fuse/shutdown analysis.

## Steady whole-cell certification

For every torque cell and applicable speed/Vdc/temperature region:

1. exercise both torque boundaries and dense interior points;
2. include torque and speed uncertainty crossing zero;
3. include exact zero, standing launch, creep, stall, wheelspin onset, regrip, four quadrants, field weakening, low Vdc, hot inverter/motor, and low-current authority;
4. include expected torque-tracking and command-quantization error;
5. build constant lower/upper pack-current bounds that cover the entire cell and operating region;
6. add source-specific measurement uncertainty only to the measured reference comparison, not as a direction deadband;
7. add separately justified numeric, aging, auxiliary, and release reserves;
8. run dense interior and adjacent-cell checks;
9. reject export when any holdout sample falls outside the applicable bound.

The certification report shall show the contribution of each margin and prove no margin is counted twice.

## Input-age and region-cap certification

Measure or derive:

- maximum positive acceleration including wheelspin;
- maximum negative acceleration including regrip;
- sensor/filter/timestamp uncertainty;
- accepted speed, Vdc, and temperature ages;
- worst number of torque and operating regions touched.

Verify that every approved operating case fits:

```text
ECU_CURRENT_MODEL_MAX_TORQUE_REGIONS_PER_POINT
ECU_CURRENT_MODEL_MAX_TOTAL_REGIONS_PER_POINT
```

Any case exceeding the compiled cap is outside the vehicle-certified domain and must fail closed.

## Settled-tracking and microstep campaign

The tracking shortcut is licensed only by measured domination.

For candidate band sizes, step sizes, and rates:

1. begin from a physically settled same-sign operating point;
2. apply repeated 100 Hz microsteps at minimum/nominal/maximum cadence and jitter;
3. exercise both torque directions and both increase/decrease directions;
4. test band-edge dither, monotonic drift, alternating dither, and long sequences;
5. repeat across speed, Vdc, temperature, and torque cells;
6. verify sign and zero crossings are excluded from the shortcut;
7. compare every measured transient with the exact committed-torque steady interval widened only by the candidate microstep margin.

Choose:

- `tracking_band_nm`;
- `maximum_microstep_nm`;
- `maximum_settled_command_rate_nm_per_s`;
- `maximum_anchor_deviation_nm`;
- `maximum_cumulative_drift_nm`;
- positive and negative microstep current margins;
- settled-tracking time and confirmation count.

Acceptance requires zero holdout violations. A larger driveability band that fails domination shall be rejected rather than hidden by an arbitrary wider residual threshold.

## Transition command grammar

The composition claim applies only to a formally bounded command grammar. Freeze:

- nominal 100 Hz command period;
- maximum early/late jitter;
- maximum normal upward/downward slew;
- immediate zero/reduction behavior;
- torque quantization;
- maximum command gap before timeout;
- permitted profile changes;
- candidate-source changes;
- maximum active duration before the path must settle or fault.

Sequences outside this grammar are not certified and must enter zero/degraded behavior.

## Transition domination campaign

For every profile, direction, span band, and operating region, test:

- one full step;
- uniform and nonuniform staircases;
- increase followed by a mid-flight reduction;
- repeated reductions;
- same-sign reapplication after a zero command while physical zero is not yet confirmed;
- candidate-source changes;
- band-scale and cell-scale dither;
- maximum command-period and CAN-delay jitter;
- immediate zero;
- reversal first leg;
- wheelspin/regrip during an active transition.

Example sequences for a 0–60 Nm span include:

```text
0 -> 60
0 -> 20 -> 40 -> 60
0 -> 15 -> 45 -> 60
0 -> 60 -> 50
0 -> 40 -> 20 -> 50
0 -> 30 -> 25 -> 45 -> 60
```

Acceptance criterion:

> Every time-aligned canonical pack-current sample from every approved trajectory shall remain inside the absolute envelope certified for the complete active raw span, after only documented measurement tolerance.

Use separate transition training, margin-selection, and composed-sequence holdout datasets.

## Transition monotonicity

For equal profile/direction/domain, a larger span shall:

- have a lower bound no greater than the smaller-span lower bound;
- have an upper bound no less than the smaller-span upper bound;
- have a maximum settling time no shorter than the smaller-span settling time.

The export tool and embedded boot validator shall both reject violations.

## Settling-time extraction

Settling begins at the last material command change and ends only after:

- elapsed time exceeds the profile/span/operating-region bound;
- exact committed-torque current lies inside the steady interval;
- command rate is below the settled threshold;
- command remains inside the tracking band;
- the required consecutive confirmation samples pass.

Extract worst-case settling by profile, direction, span, speed, Vdc, and temperature. Include torque and pack-current decay after a late authority zero and CM200 command timeout.

## Residual-monitor calibration

Measure:

- canonical current sample age;
- AMS acquisition, filtering, publication, CAN, and ECU receive-time offset, because protocol v2 does not carry a physical current-sample timestamp;
- command-to-current alignment error;
- source-switch settling behavior;
- expected consecutive transient excursions;
- false-positive rate under valid operation;
- detection latency for injected model/sensor/auxiliary faults.

Freeze:

- maximum measurement age;
- maximum command/measurement alignment error;
- source-settling sample count;
- trip and clear persistence;
- latch policy.

The monitor reports aggregate total-envelope failure unless auxiliary current is independently measured.

## Faulted auxiliary and fuse analysis

Do not treat a branch fuse rating as a current ceiling. Analyze:

- source/fault impedance;
- fuse time-current curve;
- pre-arcing and clearing `I²t`;
- conductor/PCB withstand;
- whether the fault return crosses the canonical current sensor;
- expected residual-monitor and shutdown response.

This is separate from the healthy R2D auxiliary constant.

## Target WCET and stack campaign

Use the maximum calibration schema accepted by the loader:

```text
21 torque-axis points / 20 cells
maximum operating-region count
maximum transition refinement path
active composed transition
```

On STM32F767, measure:

- steady and transition evaluator cycles;
- complete clamp cycles;
- worst interrupt interference;
- cache/compiler effects;
- APPS and CAN task stack high-water marks;
- mailbox wait and protected commit duration;
- deadline-overrun response.

Verify source counters remain within:

```text
steady calls        <= 32
transition calls    <= 8
cell evaluations    <= 64
refinements         <= 4
commit model calls  = 0
```

The 10 ms deadline and engineering target shall be approved only from target evidence.

## HIL and fault injection

Required cases include:

- stale/invalid speed, Vdc, temperature, and AMS authority;
- region and schema overflow;
- calibration corruption and generation change;
- DCL/CCL tightening during APPS computation and after software queueing;
- CM200 capability change before hardware commit;
- CAN mailbox delay/failure;
- command task timeout;
- `+T -> 0 -> -T` reversal;
- microstep drift and re-anchor;
- source-epoch change;
- stale/invalid current measurement;
- residual fault latching;
- CM200 command timeout and watchdog/shutdown paths.

## Release acceptance

Vehicle validation requires all of the following:

- zero holdout violations for approved steady, transition, and microstep domains;
- monotonic transition envelopes;
- no schema or region-cap overflow in the approved domain;
- Python/C differential agreement;
- clean target build, map, stack, and WCET records;
- measured CM200 command timeout and torque/current decay;
- HIL late-tightening, reversal, stale-input, overrun, and mailbox-failure tests;
- restricted dyno/vehicle validation;
- explicit review approval of the margin ledger and calibration artifact.
