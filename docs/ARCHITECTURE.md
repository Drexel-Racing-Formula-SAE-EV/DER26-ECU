# ECU Architecture

## Responsibility boundary

The DER26 ECU is the vehicle-facing supervisory controller. Its major responsibilities are:

- acquire APPS, brake, coolant, and protected discrete inputs;
- manage ready-to-drive and supporting vehicle-state logic;
- supervise AMS battery/power authority over CAN;
- supervise CM200 inverter state and torque capability;
- generate and constrain the requested torque command;
- control supporting low-voltage functions such as the coolant pump;
- publish diagnostics/dashboard state;
- log decoded vehicle data and accepted raw CAN frames to SD storage.

The ECU does **not** replace the CM200's internal motor current, field-oriented, voltage, thermal, or hardware-protection loops. It supplies a bounded high-level torque command.

## Torque/control flow

```text
APPS / brake / discretes
          |
          v
+---------------------+
| input/task logic    |
+---------------------+
          |
          v
 candidate torque intent
          |
          +------------------------+
          |                        |
          v                        v
+---------------------+   +----------------------+
| AMS consumer        |   | CM200 supervision    |
| power authority     |   | capability / faults  |
+---------------------+   +----------------------+
          |                        |
          +-----------+------------+
                      v
          +-------------------------+
          | pack-current/torque     |
          | model + clamp           |
          +-------------------------+
                      |
             final revalidation
                      |
                      v
              bxCAN mailbox commit
                      |
                      v
                    CM200
```

The final CAN hardware commit is a meaningful state-transition boundary. Command state should advance only when the hardware path accepts the command, and late loss of authority must invalidate a previously generated nonzero candidate.

## Task model

The application uses FreeRTOS with task ownership split by functional responsibility. The current task set includes:

- `apps_task.c` — accelerator input processing/plausibility.
- `bse_task.c` — brake sensing/plausibility.
- `bppc_task.c` — protected input/state handling.
- `rtd_task.c` — ready-to-drive state logic.
- `canbus_task.c` — CAN supervision, command publication, protocol consumers.
- `cool_task.c` — coolant sensing/control.
- `dashboard_task.c` — dashboard-facing state.
- `cli_task.c` — service diagnostics.
- `error_task.c` — fault aggregation/safety supervision.
- `acc_task.c` — supporting acquisition/state path retained by the current project.

## Power-model boundary

`Core/Src/power/` is deliberately separated from task scheduling and device drivers. It owns:

- calibrated pack-current prediction surfaces;
- bounded torque-to-current reasoning;
- torque clamp state;
- measured-vs-predicted current residual monitoring.

AMS source identity is diagnostic. The ECU consumes canonical power limits/current metadata rather than changing equations based on whether the AMS internally selected DHAB or another validated source.

## Communications

The ECU uses bxCAN at the project-defined DER26 CAN contract rate. Important communication classes include:

- AMS compact status/electrical/thermal/health and power-authority frames;
- CM200 state, capability, temperature, speed, and fault feedback;
- ECU command/diagnostic traffic;
- passive SD logging of every CAN frame accepted by the hardware filter while logging is active.

v2.10.7 explicitly wires the dedicated `CAN1_SCE` status/error interrupt in addition to TX and RX0 so CAN error/bus-off handling is serviced by the intended interrupt path.

## Generated/vendor boundary

`Drivers/`, `Middlewares/`, and much of `FATFS/` are vendor/generated support. Project-authored behavior is concentrated under `Core/`, `host_tests/`, `ci/`, `Tools/`, and `docs/`.
