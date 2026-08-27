# ECU Safety Model

## Principle: nonzero torque authority must be earned continuously

The ECU is designed so questionable or stale information removes nonzero torque permission rather than being interpreted optimistically.

The software sits inside a layered vehicle safety architecture:

1. independent hardwired shutdown circuitry;
2. AMS accumulator safety and battery-power authority;
3. ECU driver-input, vehicle-state, current-model, and inverter supervision;
4. CM200 internal inverter/motor protections.

Software is an additional permission layer, not a substitute for the electrical shutdown system.

## Candidate generation vs hardware commit

The ECU separates producing a candidate torque command from committing that command to the CAN peripheral. Before nonzero torque is accepted into the hardware path, the ECU rechecks the authority inputs that can invalidate the candidate.

Representative requirements include:

- valid/plausible driver inputs;
- valid protected discrete inputs;
- fresh/coherent AMS power authority;
- CM200 torque-mode/state/capability health;
- pack-current model limits;
- no latched current-residual violation requiring zero torque;
- final command freshness and commit-time coherence.

Zero torque remains the deterministic fallback when required authority is missing.

## CAN safety behavior

CAN supervision includes freshness, sequence/coherency, mailbox ownership, and error handling. The v2.10.7 source explicitly enables TX, RX0, and SCE interrupt paths so status/error notifications are not serviced only opportunistically by unrelated CAN traffic.

A generic CAN error must not silently retire unrelated TX ownership. Command state should track actual mailbox outcomes.

## Panic/safe outputs

Safety/panic paths directly force propulsion-related outputs to a fail-low state. Supporting actuators that require a different hardware-safe behavior, such as the coolant pump's defined fail-safe state, are handled explicitly rather than relying on normal task execution after a panic.

## Build profiles and evidence gates

Bench-oriented builds intentionally inhibit propulsion authority while retaining sensing, CAN, logging, diagnostics, and controlled bench functionality.

Vehicle authority requires explicit validation acknowledgements for items such as:

- BSPD/other hardware interface levels;
- pin map and discrete inputs;
- APPS and brake calibration;
- AMS and CM200 protocol contracts;
- pack-current model and residual monitor;
- cooling behavior;
- RTOS memory/stack margin;
- WCET and interrupt interaction;
- CAN loading/error recovery;
- watchdog behavior;
- safe-output behavior.

Those macros represent evidence already obtained. Setting a macro is not itself validation.

## What host tests cannot prove

Host tests and static gates cannot prove:

- real Cortex-M7 WCET or scheduling jitter;
- real task/ISR stack margins;
- physical CAN error/noise timing;
- sensor calibration accuracy;
- electrical input/output levels;
- watchdog behavior on target;
- pump/inverter/vehicle plant behavior;
- EMI/grounding effects.

Those remain target, HIL, dyno, and vehicle-validation responsibilities.
