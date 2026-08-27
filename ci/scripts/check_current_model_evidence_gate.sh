#!/usr/bin/env bash
set -euo pipefail
ROOT="${1:-.}"
MODEL_H="$ROOT/Core/Inc/power/ecu_pack_current_model.h"
MODEL_C="$ROOT/Core/Src/power/ecu_pack_current_model.c"
CAL_C="$ROOT/Core/Src/power/ecu_pack_current_calibration.c"
CONFIG_H="$ROOT/Core/Inc/ecu_config.h"

grep -q 'bool evidence_valid;' "$MODEL_H"
grep -q '!cal->evidence_valid' "$MODEL_C"
grep -q '(cal->crc32 == 0u)' "$MODEL_C"
grep -q '\.evidence_valid = false' "$CAL_C"
grep -q '\.crc32 = 0u' "$CAL_C"
grep -q '#define ECU_AMS_POWER_CLAMP_VALIDATED 0' "$CONFIG_H"

echo "PASS current-model evidence, CRC, and vehicle-validation gates remain fail-closed"
