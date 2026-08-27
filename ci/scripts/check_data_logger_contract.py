#!/usr/bin/env python3
"""Static contract gate for the ECU SD/CAN data logger."""
from __future__ import annotations

import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
logger = (root / "Core/Src/ext_drivers/ecu_data_logger.c").read_text()
logger_h = (root / "Core/Inc/ext_drivers/ecu_data_logger.h").read_text()
app_h = (root / "Core/Inc/app.h").read_text()
app_c = (root / "Core/Src/app.c").read_text()
irq = (root / "Core/Src/stm32f7xx_it.c").read_text()
canbus = (root / "Core/Src/ext_drivers/canbus.c").read_text()
cli = (root / "Core/Src/tasks/cli_task.c").read_text()
sd = (root / "Core/Src/ext_drivers/sdcard_service.c").read_text()
ams = (root / "Core/Src/ext_drivers/ams.c").read_text()
ams_h = (root / "Core/Inc/ext_drivers/ams.h").read_text()
ffconf = (root / "FATFS/Target/ffconf.h").read_text()
errors: list[str] = []


def require(text: str, pattern: str, label: str, *, regex: bool = False) -> None:
    found = re.search(pattern, text, re.MULTILINE | re.DOTALL) if regex else pattern in text
    if not found:
        errors.append(label)


def function_body(text: str, signature: str) -> str:
    start = text.find(signature)
    if start < 0:
        return ""
    brace = text.find("{", start)
    if brace < 0:
        return ""
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[brace + 1:i]
    return ""

require(app_h, "#define LOGGER_PRIO 2", "logger must remain below all control-task priorities")
require(app_h, "#define ECU_STACK_LOGGER_WORDS    1536u", "logger static stack budget changed without review")
require(logger, "xTaskCreateStatic(logger_task_fn", "logger task must use static allocation")
require(app_c, "app.logger_task = ecu_data_logger_task_start(&app);", "application must create logger task")
require(logger, "LOGGER_RAW_RING_CAPACITY 512u", "raw ISR ring capacity must remain explicit")
require(logger, "_Static_assert(sizeof(logger_raw_record_t) == 24u", "raw record ABI must remain guarded")
require(logger, "_Static_assert(sizeof(decoded_snapshot_t) <= 256u",
        "decoded logger snapshot must retain a bounded critical-copy size")
if "s->ams = app->board.ams" in logger or "s->cm200 = app->board.cm200" in logger:
    errors.append("logger must not copy complete AMS/CM200 protocol objects while interrupts are masked")
snapshot_body = function_body(logger, "static void capture_snapshot")
if snapshot_body:
    memset_pos = snapshot_body.find("memset(s, 0, sizeof(*s));")
    critical_pos = snapshot_body.find("taskENTER_CRITICAL();")
    if (memset_pos < 0) or (critical_pos < 0) or (memset_pos > critical_pos):
        errors.append("decoded snapshot memset must occur before interrupts are masked")
else:
    errors.append("cannot locate decoded snapshot capture function")
require(logger, "LOGGER_SYNC_PERIOD_MS 1000u", "periodic file sync contract missing")
require(logger, "FA_CREATE_NEW | FA_WRITE", "session files must never silently overwrite prior logs")
require(logger, "g_logger.ring.dropped++", "ISR overflow must be counted")
require(logger, "if(next == g_logger.ring.tail)", "ISR ring must drop on full rather than block")
require(logger, "g_logger.capture_active = false;", "stop path must disable the ISR producer before closing")
require(logger, "flush_raw_locked();\n    event_locked(\"STOP\"", "stop path must drain raw records before closing")
require(logger, "sdcard_service_lock", "logger filesystem access must use SD service ownership")
require(sd, "xSemaphoreCreateRecursiveMutexStatic", "FatFs ownership must use a static recursive mutex")
require(cli, "stop the data logger before SD tests or unmount", "CLI must prevent logger/filesystem collisions")
require(canbus, "0x680u", "CAN range filter for AMS logger frames missing")
require(canbus, "0x780u", "CAN range filter mask for 0x680-0x6FF missing")
require(irq, "ecu_data_logger_can_rx_isr", "CAN RX path must feed raw logger")
require(irq, "can_rx_isr_max_cycles", "CAN RX ISR must publish target WCET evidence")
require(irq, "can_rx_isr_budget_exhaust_count",
        "CAN RX ISR must count bounded-drain backlog events")
require(logger_h, "Non-safety SD/CAN logger", "logger safety classification missing")
require(ams_h, "Passive AMS diagnostic/logger range", "AMS logger range must remain explicitly passive")

isr_body = function_body(logger, "void ecu_data_logger_can_rx_isr")
if not isr_body:
    errors.append("cannot locate raw CAN ISR function")
else:
    forbidden = ("f_write", "f_sync", "f_open", "sdcard_service_lock", "osDelay", "xSemaphore")
    for token in forbidden:
        if token in isr_body:
            errors.append(f"raw CAN ISR must not call {token}")

# The passive logger parser may populate observability fields, but none of those
# fields may be used by the torque-authority function.
torque_body = function_body(ams, "bool ams_allows_torque")
for token in ("logger_protocol_version", "logger_sequence", "logger_snapshot_sequence",
              "logger_phase", "apm_current1_0p01a", "apm_flags"):
    if token in torque_body:
        errors.append(f"passive logger field {token} leaked into torque authority")

if re.search(r"\b(malloc|calloc|realloc|free)\s*\(", logger):
    errors.append("data logger must not use heap allocation")

require(logger, "const char *names[]", "session chooser must inspect all session artifacts")
require(logger, "g_logger.decoded_name", "session chooser must account for decoded orphan files")
require(logger, "g_logger.raw_name", "session chooser must account for raw orphan files")
require(logger, "g_logger.event_name", "session chooser must account for event orphan files")
require(logger, "g_logger.meta_name", "session chooser must account for metadata files")
require(logger, "LOGGER_MAX_CONSECUTIVE_ERROR_CYCLES", "logger must bound repeated filesystem failures")

# The logger intentionally keeps decoded/raw/event/meta files open for the session.
# FatFs _FS_LOCK must therefore be >=4 or the third/fourth f_open fails with
# FR_TOO_MANY_OPEN_FILES on real hardware even though host fakes pass.
m = re.search(r"#define\s+_FS_LOCK\s+(\d+)", ffconf)
if (m is None) or (int(m.group(1)) < 4):
    errors.append("FatFs _FS_LOCK must be >=4 for the four-file logger session")
require(logger, "requeue_request_if_none", "logger retries must not overwrite newer operator requests")

if errors:
    print("ERROR ECU data-logger static contract gate")
    for error in errors:
        print(f"- {error}")
    sys.exit(1)

print("PASS ECU logger is static, non-blocking in CAN RX, filesystem-serialized, and non-gating")
