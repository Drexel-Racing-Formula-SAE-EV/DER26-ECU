# DER26 ECU Firmware v2.6.3 Release Notes

## Purpose

v2.6.3 closes the actionable findings from the independent Revision 7 audit
without redesigning the torque clamp.

## Important audit correction

The reviewed v2.6.2 source already connected a latched current-model residual to
torque authority in both places that matter:

- APPS candidate generation includes `current_model_residual_fault` in the hard
  torque gate.
- The CAN task's final protected commit includes the same fault in its late
  safety recheck.

The residual therefore already forced zero/disable on the next protected commit
and prevented subsequent nonzero candidates. The actual remaining defect was
that the dedicated `ECU_CLAMP_REASON_CURRENT_MODEL_RESIDUAL` value was not
assigned, leaving fault attribution generic and allowing the wiring to regress
without a focused CI gate.

The checked-in current-model calibration also already contains an
`evidence_valid` field, CRC qualification, an invalid default artifact, and a
vehicle validation gate. v2.6.3 adds a source gate so those protections cannot
silently disappear.

## Changes

- Explicitly assign `ECU_CLAMP_REASON_CURRENT_MODEL_RESIDUAL` in APPS and final
  CAN commit paths.
- Freeze the release policy: no calibrated independent degraded cap exists, so
  a latched residual forces zero/disable.
- Add a static residual-authority regression gate.
- Add a static calibration-evidence/CRC regression gate.
- Add the independent Revision 7 invariant probe to normal CI.
- Add the power-bundle availability probe to normal CI.
- Enforce <=120 ms outage after one dropped 10 Hz bundle and <=160 ms after two.
- Add a compile-time stale-window/full-counter-wrap guard.
- Add an explicit torque-removal and authority-availability budget document.
- Update GitHub CI, project-structure checks, sanitizers, and analyzers for the
  new probes.

## Safety status

The checked-in current-model calibration remains invalid and cannot authorize
nonzero vehicle torque. Cooling, target WCET, stack, physical pin, bus-load,
HIL/dyno, and HV validation gates remain open.
