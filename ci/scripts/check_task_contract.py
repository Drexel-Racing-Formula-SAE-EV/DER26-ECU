#!/usr/bin/env python3
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
app = (root / "Core/Src/app.c").read_text()
task_dir = root / "Core/Src/tasks"
expected = ["cli", "rtd", "error", "canbus", "bse", "apps", "bppc", "acc", "dashboard", "cool"]
best_effort = ["logger"]
errors = []
for name in expected:
    call = f"app.{name}_task = {name}_task_start(&app);"
    if app.count(call) != 1:
        errors.append(f"app.c must start {name} exactly once")
    if f"(app.{name}_task == NULL)" not in app:
        errors.append(f"app.c startup_fault does not include {name}_task")
    src = task_dir / f"{name}_task.c"
    if not src.exists():
        errors.append(f"missing {src.relative_to(root)}")
        continue
    text = src.read_text()
    if text.count("xTaskCreateStatic(") != 1:
        errors.append(f"{src.relative_to(root)} must create exactly one static task")
    if "StaticTask_t" not in text or "StackType_t" not in text:
        errors.append(f"{src.relative_to(root)} lacks static task storage")
    if "while(1)" not in text.replace(" ", "") and "for(;;)" not in text.replace(" ", ""):
        errors.append(f"{src.relative_to(root)} has no explicit task loop")

# Best-effort logger is intentionally implemented in the driver module so its
# ISR ring and file owner remain one unit. It must start exactly once, use
# static task storage, and stay out of startup_fault/safety gating.
for name in best_effort:
    call = f"app.{name}_task = ecu_data_logger_task_start(&app);"
    if app.count(call) != 1:
        errors.append(f"app.c must start best-effort {name} exactly once")
    if f"(app.{name}_task == NULL)" in app:
        errors.append(f"best-effort {name} must not gate startup_fault")
logger_src = root / "Core/Src/ext_drivers/ecu_data_logger.c"
if not logger_src.exists():
    errors.append("missing Core/Src/ext_drivers/ecu_data_logger.c")
else:
    logger_text = logger_src.read_text()
    if logger_text.count("xTaskCreateStatic(") != 1:
        errors.append("ecu_data_logger.c must create exactly one static task")
    if "StaticTask_t" not in logger_text or "StackType_t" not in logger_text:
        errors.append("ecu_data_logger.c lacks static task storage")

bse_text = (task_dir / "bse_task.c").read_text()
if "pressure_sensor_check_failure" in bse_text:
    errors.append("BSE task must use the correctly named pressure_sensor_in_range helper")
if "pressure_sensor_in_range" not in bse_text:
    errors.append("BSE task lost pressure sensor range plausibility checks")
if bse_text.count("data->brake = 0;") < 2:
    errors.append("BSE ADC/mutex failure paths must invalidate stale brake telemetry")

if len(list(task_dir.glob("*_task.c"))) != len(expected):
    errors.append("safety task source count changed; update expected task manifest intentionally")
if errors:
    print("ERROR ECU task contract gate")
    print("\n".join(errors))
    sys.exit(1)
print("PASS 10 safety tasks are startup-gated and 1 best-effort logger task is statically created/non-gating")
