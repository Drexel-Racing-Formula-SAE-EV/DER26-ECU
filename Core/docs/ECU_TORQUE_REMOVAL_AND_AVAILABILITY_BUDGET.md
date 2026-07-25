# DER26 ECU Torque-Removal and Power-Authority Availability Budget

## Scope

This document records software-contract timing for the Revision 7 torque clamp.
It does not replace target DWT, CAN arbitration, or HIL evidence.

## Nominal scheduling assumptions

- APPS/clamp producer: 100 Hz
- Protected CAN commit: bounded command service, nominally 100 Hz or faster
- AMS power-authority producer: 10 Hz
- Power-bundle stale limit: 250 ms
- Requalification: two strict-consecutive complete bundles
- Canonical current residual: three distinct violating samples

## Residual-fault torque response

A total-envelope residual is sampled only when a new canonical AMS current
sequence is accepted. The third distinct violation latches the residual fault.
Because no independent degraded torque calibration exists, the next protected
commit forces zero/disable and reports
`ECU_CLAMP_REASON_CURRENT_MODEL_RESIDUAL`.

At a nominal 10 Hz current publication rate, the third violation arrives about
200 ms after the first violating sample. The additional software response is
one protected-commit opportunity. Physical acquisition delay, bus arbitration,
and target scheduling jitter must be measured before vehicle release.

The residual does not independently open the shutdown circuit in v2.6.3. It
removes torque authority and remains latched until reset. A future shutdown
escalation policy requires separate hazard review and validation.

## Power-bundle availability after packet loss

The consumer requires strict +1 bundle succession and two consecutive complete
bundles to requalify. Host-contract measurement at the nominal 100 ms period:

| Disturbance | Measured zero-authority outage | Regression budget |
|---|---:|---:|
| One dropped complete bundle | 110 ms | <= 120 ms |
| Two consecutive dropped bundles | 150 ms | <= 160 ms |

These values are enforced by `bundle_availability_probe.c`. Any change to the
publication period, stale limit, counter policy, or requalification depth must
update and reapprove this budget.

## Counter/staleness invariant

The consumer uses strict +1 succession, so half-range modular ordering is not
used. A compile-time guard still requires the stale window to be shorter than
one full 4-bit counter wrap at the nominal publication rate. This prevents a
future configuration from making an old same-counter bundle plausible inside
the accepted freshness window.

## Vehicle-release evidence still required

- Target CAN bus utilization and arbitration delay
- Maximum ISR and critical-section duration
- APPS and CAN task jitter
- DWT clamp and commit WCET
- Measured end-to-end AMS acquisition-to-ECU-use age
- Fault injection for dropped, delayed, duplicated, and reordered bundles
- Hardware confirmation that residual trip always produces zero/disable
