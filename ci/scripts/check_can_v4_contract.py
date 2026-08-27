#!/usr/bin/env python3
"""Static DER26-CAN-V4 ECU contract gate.

This gate intentionally checks source-level invariants that are easy to break in
future edits: protected/detail ID ordering, removal of the historical high-
priority detail IDs, explicit CAN1 filter-bank ownership, RTR rejection, bounded
CM200 commit waiting, non-blocking ECU->AMS feedback, and end-to-end detail-loss
observability.
"""
from pathlib import Path
import re
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()


def read(rel: str) -> str:
    p = root / rel
    if not p.exists():
        raise SystemExit(f"FAIL missing {rel}")
    return p.read_text(errors="replace")


def macro(text: str, name: str) -> int:
    m = re.search(rf"^\s*#\s*define\s+{re.escape(name)}\s+([^\s/]+)", text, re.M)
    if not m:
        raise SystemExit(f"FAIL missing macro {name}")
    token = m.group(1).rstrip("uUlL")
    try:
        return int(token, 0)
    except ValueError as exc:
        raise SystemExit(f"FAIL macro {name} is not a literal integer: {m.group(1)}") from exc

app = read("Core/Inc/app.h")
ams = read("Core/Inc/ext_drivers/ams.h")
power = read("Core/Inc/ext_drivers/ams_power_consumer.h")
can_h = read("Core/Inc/ext_drivers/canbus.h")
can_c = read("Core/Src/ext_drivers/canbus.c")
can_task = read("Core/Src/tasks/canbus_task.c")
isr = read("Core/Src/stm32f7xx_it.c")
logger = read("Core/Src/ext_drivers/ecu_data_logger.c")
cli = read("Core/Src/tasks/cli_task.c")

if '#define DER26_CAN_CONTRACT_NAME "DER26-CAN-V4"' not in app:
    raise SystemExit("FAIL ECU banner/contract name is not DER26-CAN-V4")

required_names = [
    "AMS_ECU_STATUS_CANBUS_ID",
    "AMS_ECU_ELECTRICAL_CANBUS_ID",
    "AMS_ECU_THERMAL_CANBUS_ID",
    "AMS_ECU_HEALTH_CANBUS_ID",
    "DER26_POWER_DCL_ID",
    "DER26_POWER_CCL_ID",
    "DER26_POWER_SOH_ID",
    "DER26_POWER_ENVELOPE_ID",
]
required_values = [
    macro(ams, required_names[0]), macro(ams, required_names[1]),
    macro(ams, required_names[2]), macro(ams, required_names[3]),
    macro(power, required_names[4]), macro(power, required_names[5]),
    macro(power, required_names[6]), macro(power, required_names[7]),
]
if required_values != list(range(0x680, 0x688)):
    raise SystemExit(f"FAIL protected required ID map is {required_values!r}, expected 0x680..0x687")

if macro(power, "DER26_MISSION_REQUEST_ID") != 0x688:
    raise SystemExit("FAIL mission request must remain ECU->AMS ID 0x688")
if macro(power, "DER26_POWER_STRATEGY_ID") != 0x689:
    raise SystemExit("FAIL strategy ID must be 0x689")
if macro(power, "DER26_POWER_BINDINGS_ID") != 0x68A:
    raise SystemExit("FAIL bindings ID must be 0x68A")
if macro(ams, "AMS_ECU_CURRENT_DIAG_CANBUS_ID") != 0x68B:
    raise SystemExit("FAIL current diagnostic/advisory ID must be 0x68B")

if macro(ams, "AMS_LOGGER_CAN_ID_FIRST") < 0x690:
    raise SystemExit("FAIL detail logger range numerically outranks protected traffic")
if macro(ams, "AMS_TELEM_CANBUS_ID") != 0x6B1:
    raise SystemExit("FAIL legacy compatibility telemetry must be relocated to 0x6B1")
if macro(ams, "AMS_ESTIMATOR_CANBUS_ID") != 0x6B2:
    raise SystemExit("FAIL estimator diagnostic must be relocated to 0x6B2")
if macro(ams, "AMS_LOGGER_CAN_ID_TX_SCHED_DIAG") != 0x6B3:
    raise SystemExit("FAIL TX scheduler diagnostic must be 0x6B3")
if macro(ams, "AMS_LOGGER_CAN_ID_ESTIMATOR_VOLTAGE_COMPARE") != 0x6B4:
    raise SystemExit("FAIL estimator voltage comparison must be 0x6B4")
tuning_names = [
    "AMS_LOGGER_CAN_ID_EKF_STATE", "AMS_LOGGER_CAN_ID_EKF_MODEL",
    "AMS_LOGGER_CAN_ID_EKF_COVARIANCE", "AMS_LOGGER_CAN_ID_EKF_SOH",
    "AMS_LOGGER_CAN_ID_EKF_CONTEXT", "AMS_LOGGER_CAN_ID_SOP_DISCHARGE",
    "AMS_LOGGER_CAN_ID_SOP_CHARGE", "AMS_LOGGER_CAN_ID_SOP_BINDING",
    "AMS_LOGGER_CAN_ID_FUSE_STATE", "AMS_LOGGER_CAN_ID_FUSE_LIMIT",
    "AMS_LOGGER_CAN_ID_TUNING_META", "AMS_LOGGER_CAN_ID_SOP_META",
]
if [macro(ams, name) for name in tuning_names] != list(range(0x6B5, 0x6C1)):
    raise SystemExit("FAIL passive tuning range must be contiguous 0x6B5..0x6C0")
if macro(ams, "AMS_LOGGER_CAN_ID_LAST") != 0x6C0:
    raise SystemExit("FAIL passive logger range must extend through 0x6C0")
if macro(ams, "AMS_ECU_DIAG_FEEDBACK_CAN_ID") != 0x6F0:
    raise SystemExit("FAIL ECU->AMS diagnostic feedback must be 0x6F0")

# The historical priority-inversion IDs must not remain active definitions.
if re.search(r"^\s*#\s*define\s+AMS_TELEM_CANBUS_ID\s+0x0*69[uUlL]*\b", ams, re.M):
    raise SystemExit("FAIL production compatibility telemetry still uses high-priority ID 0x069")
if re.search(r"^\s*#\s*define\s+AMS_ESTIMATOR_CANBUS_ID\s+0x0*421[uUlL]*\b", ams, re.M):
    raise SystemExit("FAIL estimator detail still uses high-priority ID 0x421")

# CAN1 filter banks 0..13 are owned by CAN1; this implementation consumes 0..7.
for required_text in [
    "CANBUS_CAN1_FILTER_BANK_FIRST 0u",
    "CANBUS_CAN1_FILTER_BANK_SPLIT 14u",
    "CANBUS_AMS_RANGE_FILTER_BANK  CANBUS_FILTER_BANK_COUNT",
    "filter.SlaveStartFilterBank = CANBUS_CAN1_FILTER_BANK_SPLIT",
    "filter.FilterMaskIdLow = 0x0006u",
]:
    if required_text not in can_c:
        raise SystemExit(f"FAIL CAN1 filter/RTR contract missing: {required_text}")

if "rx_header.RTR == CAN_RTR_DATA" not in isr or "rx_remote_count++" not in isr:
    raise SystemExit("FAIL ECU RX ISR lacks explicit software RTR rejection/counting")

# CM200 final commit wait must be finite at both supported bitrates.
if "CANBUS_CM200_COMMIT_WAIT_MS 2u" not in can_h or "CANBUS_CM200_COMMIT_WAIT_MS 4u" not in can_h:
    raise SystemExit("FAIL bounded 500k/250k CM200 commit waits are missing")
if "canbus_wait_tx_ready(canbus, CANBUS_CM200_COMMIT_WAIT_MS)" not in can_task:
    raise SystemExit("FAIL CM200 task does not use bounded final-commit mailbox wait")
if "cm200_command_torque_0p1nm = 0" not in can_task:
    raise SystemExit("FAIL CM200 wait expiry lacks local fail-zero state")

# Mailbox enqueue is not a physical commit. The torque-clamp state and rolling
# counter may advance only after the bxCAN TX-complete callback retires the
# tracked mailbox token.
for required_text in [
    "CANBUS_CM200_TX_COMPLETE_TIMEOUT_MS 4u",
    "canbus_transmit_ready_tracked", "canbus_wait_tx_complete",
    "canbus_tx_complete_isr", "canbus_tx_abort_isr",
]:
    if required_text not in (can_h + can_c + can_task + isr):
        raise SystemExit(f"FAIL true CM200 TX completion contract missing: {required_text}")
enqueue_pos = can_task.find("canbus_transmit_ready_tracked")
complete_pos = can_task.find("canbus_wait_tx_complete", enqueue_pos)
commit_pos = can_task.find("ecu_torque_clamp_note_hardware_commit", complete_pos)
counter_pos = can_task.find("cm200_rolling_counter =", complete_pos)
if not (0 <= enqueue_pos < complete_pos < commit_pos < counter_pos):
    raise SystemExit("FAIL torque/counter state advances before actual TX completion")
for callback in [
    "HAL_CAN_TxMailbox0CompleteCallback", "HAL_CAN_TxMailbox1CompleteCallback",
    "HAL_CAN_TxMailbox2CompleteCallback", "HAL_CAN_TxMailbox0AbortCallback",
    "HAL_CAN_TxMailbox1AbortCallback", "HAL_CAN_TxMailbox2AbortCallback",
]:
    if callback not in isr:
        raise SystemExit(f"FAIL bxCAN callback missing: {callback}")

# Feedback must be observational and non-blocking: immediate mailbox check + direct enqueue.
feedback_start = can_task.find("static void canbus_try_send_ams_feedback")
feedback_end = can_task.find("TaskHandle_t canbus_task_start", feedback_start)
if feedback_start < 0 or feedback_end < 0:
    raise SystemExit("FAIL ECU->AMS feedback implementation missing")
feedback = can_task[feedback_start:feedback_end]
for required_text in [
    "HAL_CAN_GetTxMailboxesFreeLevel",
    "feedback_tx_deferred_count",
    "canbus_transmit_ready",
    "AMS_ECU_DIAG_FEEDBACK_CAN_ID",
]:
    if required_text not in feedback:
        raise SystemExit(f"FAIL feedback nonblocking contract missing: {required_text}")
if "canbus_wait_tx_ready" in feedback or "osDelay" in feedback or "taskYIELD" in feedback:
    raise SystemExit("FAIL ECU->AMS observability feedback contains a blocking/waiting TX path")

# Post-run logger must preserve source and receive-side loss evidence.
for field in [
    "logger_snapshot_gap_count",
    "logger_incomplete_snapshot_count",
    "logger_phase_gap_count",
    "logger_cell_fragment_gap_count",
    "logger_temp_fragment_gap_count",
    "logger_duplicate_fragment_count",
    "logger_out_of_order_count",
    "tx_sched_protected_deadline_miss",
    "tx_sched_detail_superseded",
    "tx_sched_detail_recovery_discard",
]:
    if field not in ams:
        raise SystemExit(f"FAIL AMS decoder lacks observable CAN-V4 field {field}")
for token in [
    "AMS_SNAPSHOT_GAP",
    "AMS_DETAIL_INCOMPLETE",
    "AMS_SOURCE_SUPERSEDE",
    "AMS_PROTECTED_DEADLINE",
]:
    if token not in logger:
        raise SystemExit(f"FAIL ECU event logger lacks {token} visibility")

if "DER26_CAN_CONTRACT_NAME" not in cli or "DER26_CAN_BITRATE_KBPS" not in cli:
    raise SystemExit("FAIL CLI/version output does not expose CAN-V4 contract/bitrate")

print("PASS DER26-CAN-V4 ECU ID/filter/commit/feedback/logger contract")
