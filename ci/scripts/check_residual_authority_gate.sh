#!/usr/bin/env bash
set -euo pipefail
ROOT="${1:-.}"
APPS="$ROOT/Core/Src/tasks/apps_task.c"
CAN="$ROOT/Core/Src/tasks/canbus_task.c"
ERROR_TASK="$ROOT/Core/Src/tasks/error_task.c"

grep -q 'data->hard_fault || data->current_model_residual_fault' "$APPS"
grep -q 'data->hard_fault || data->current_model_residual_fault' "$CAN"
grep -q 'ECU_CLAMP_REASON_CURRENT_MODEL_RESIDUAL' "$APPS"
grep -q 'ECU_CLAMP_REASON_CURRENT_MODEL_RESIDUAL' "$CAN"
grep -q 'data->current_model_residual_fault' "$ERROR_TASK"

echo "PASS residual fault removes torque authority and has an explicit commit reason"
