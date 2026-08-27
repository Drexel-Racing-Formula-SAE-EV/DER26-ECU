#!/usr/bin/env python3
"""Decode DER26 ECU CAN###.BIN files into portable CSV files.

The primary CSV preserves every raw record.  Additional long-form AMS files are
created for cell voltages, temperatures, snapshot metadata, and ADBMS2950/APM
telemetry so the phased logger stream can be analyzed without custom parsing.
"""
from __future__ import annotations

import argparse
import csv
import json
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

HEADER_SIZE = 32
RECORD = struct.Struct("<IIIBB8sH")

ID_NAMES = {
    # Historical pre-V4 IDs are retained only so older logs remain decodable.
    0x069: "AMS_LEGACY_PAGED_PRE_V4",
    0x421: "AMS_ESTIMATOR_PRE_V4",
    0x680: "AMS_COMPACT_STATUS",
    0x681: "AMS_COMPACT_ELECTRICAL",
    0x682: "AMS_COMPACT_THERMAL",
    0x683: "AMS_COMPACT_HEALTH",
    0x684: "AMS_POWER_DCL",
    0x685: "AMS_POWER_CCL",
    0x686: "AMS_POWER_SOH",
    0x687: "AMS_POWER_ENVELOPE",
    0x689: "AMS_POWER_STRATEGY",
    0x68A: "AMS_POWER_BINDINGS",
    0x68B: "AMS_CURRENT_SOURCE_DIAG",
    0x690: "AMS_LOG_HEARTBEAT",
    0x691: "AMS_LOG_FAULT_REASONS",
    0x692: "AMS_LOG_PACK_ELECTRICAL",
    0x693: "AMS_LOG_TEMP_FAN",
    0x694: "AMS_LOG_VOLTAGE_HEALTH",
    0x695: "AMS_LOG_TEMP_HEALTH",
    0x696: "AMS_LOG_CHARGER",
    0x697: "AMS_LOG_CURRENT_DETAIL",
    0x698: "AMS_LOG_6830_LINK",
    0x699: "AMS_LOG_6830_COUNTERS",
    0x69A: "AMS_LOG_2950_LINK",
    0x69B: "AMS_LOG_TASK_HEALTH",
    0x69C: "AMS_LOG_CAN_DIAG",
    0x69D: "AMS_LOG_SAFETY_DIAG",
    0x69E: "AMS_LOG_WATCHDOG_DIAG",
    0x69F: "AMS_LOG_ADBMS_DIAG",
    0x6A0: "AMS_LOG_CELL_DETAIL",
    0x6A1: "AMS_LOG_TEMP_DETAIL",
    0x6A2: "AMS_LOG_VOLTAGE_MASKS",
    0x6A3: "AMS_LOG_TEMP_MASKS_A",
    0x6A4: "AMS_LOG_TEMP_MASKS_B",
    0x6A5: "AMS_LOG_VOLTAGE_PEC",
    0x6A6: "AMS_LOG_CURRENT_ADC",
    0x6A7: "AMS_LOG_CHARGER_DETAIL",
    0x6A8: "AMS_LOG_TEMP_DIAG",
    0x6A9: "AMS_LOG_TEMP_DIAG_A",
    0x6AA: "AMS_LOG_TEMP_DIAG_B",
    0x6AB: "AMS_LOG_VOLTAGE_DIAG",
    0x6AC: "AMS_LOG_RTOS_DIAG",
    0x6AD: "AMS_LOG_SNAPSHOT_META",
    0x6AE: "AMS_LOG_APM_SAMPLE",
    0x6AF: "AMS_LOG_APM_HEALTH",
    0x6B0: "AMS_LOG_APM_RAW",
    0x6B1: "AMS_LEGACY_PAGED_COMPAT_V4",
    0x6B2: "AMS_ESTIMATOR_V4",
    0x6B3: "AMS_LOG_TX_SCHEDULER",
    0x6B4: "AMS_ESTIMATOR_VOLTAGE_COMPARE",
    0x6B5: "AMS_TUNING_EKF_STATE",
    0x6B6: "AMS_TUNING_EKF_MODEL",
    0x6B7: "AMS_TUNING_EKF_COVARIANCE",
    0x6B8: "AMS_TUNING_EKF_SOH",
    0x6B9: "AMS_TUNING_EKF_CONTEXT",
    0x6BA: "AMS_TUNING_SOP_DISCHARGE",
    0x6BB: "AMS_TUNING_SOP_CHARGE",
    0x6BC: "AMS_TUNING_SOP_BINDING",
    0x6BD: "AMS_TUNING_FUSE_STATE",
    0x6BE: "AMS_TUNING_FUSE_LIMIT",
    0x6BF: "AMS_TUNING_META",
    0x6C0: "AMS_TUNING_SOP_META",
    0x6F0: "ECU_AMS_DIAG_FEEDBACK",
}


def be_u16(data: bytes, offset: int = 0) -> int:
    return int.from_bytes(data[offset:offset + 2], "big", signed=False)


def be_i16(data: bytes, offset: int = 0) -> int:
    return int.from_bytes(data[offset:offset + 2], "big", signed=True)


def be_u24(data: bytes, offset: int = 0) -> int:
    return int.from_bytes(data[offset:offset + 3], "big", signed=False)


def be_u32(data: bytes, offset: int = 0) -> int:
    return int.from_bytes(data[offset:offset + 4], "big", signed=False)


def be_i32(data: bytes, offset: int = 0) -> int:
    return int.from_bytes(data[offset:offset + 4], "big", signed=True)


def valid_i16(value: int) -> int | None:
    return None if value == -32768 else value


def valid_u16(value: int) -> int | None:
    return None if value == 0xFFFF else value


@dataclass
class SnapshotContext:
    version: int | None = None
    logger_sequence: int | None = None
    phase: int | None = None
    phase_count: int | None = None
    measurement_sequence: int | None = None


def decode_fields(can_id: int, data: bytes) -> dict[str, Any]:
    """Decode high-value AMS fields while preserving raw bytes for every ID."""
    if can_id == 0x681:
        return {
            "pack_voltage_0p1V": be_u16(data, 0),
            "pack_current_0p1A": be_i16(data, 2),
            "min_cell_mV": be_u16(data, 4),
            "max_cell_mV": be_u16(data, 6),
        }
    if can_id == 0x682:
        return {
            "max_temp_0p1C": be_i16(data, 0),
            "min_temp_0p1C": be_i16(data, 2),
            "avg_temp_0p1C": be_i16(data, 4),
            "max_fan_pct": data[6],
            "flags": data[7],
        }
    if can_id == 0x683:
        return {
            "max_cell_segment": data[0], "max_cell_index": data[1],
            "min_cell_segment": data[2], "min_cell_index": data[3],
            "max_temp_segment": data[4], "max_temp_index": data[5],
            "usable_cell_count": data[6], "usable_temp_count": data[7],
        }
    if can_id == 0x68B:
        return {
            "source": data[0], "quality": data[1],
            "physical_boundary_valid": data[2], "source_epoch": data[3],
            "sample_sequence": be_u16(data, 4), "sample_age_ms": be_u16(data, 6),
        }
    if can_id == 0x6A0:
        return {
            "segment": data[0], "start_cell": data[1],
            "cell0_mV": valid_u16(be_u16(data, 2)),
            "cell1_mV": valid_u16(be_u16(data, 4)),
            "cell2_mV": valid_u16(be_u16(data, 6)),
        }
    if can_id == 0x6A1:
        return {
            "segment": data[0], "start_sensor": data[1],
            "temp0_0p1C": valid_i16(be_i16(data, 2)),
            "temp1_0p1C": valid_i16(be_i16(data, 4)),
            "temp2_0p1C": valid_i16(be_i16(data, 6)),
        }
    if can_id == 0x6A2:
        return {
            "segment": data[0], "updated_mask": be_u16(data, 1),
            "usable_mask": be_u16(data, 3), "stale_mask": be_u16(data, 5),
        }
    if can_id == 0x6A3:
        return {
            "segment": data[0], "updated_mask": be_u24(data, 1),
            "usable_mask": be_u24(data, 4),
        }
    if can_id == 0x6A4:
        return {
            "segment": data[0], "stale_mask": be_u24(data, 1),
            "invalid_mask": be_u24(data, 4),
        }
    if can_id == 0x6A5:
        return {
            "segment": data[0], "pec_mask": be_u16(data, 1),
            "pec_count": data[3],
        }
    if can_id == 0x6A9:
        return {
            "segment": data[0], "open_mask": be_u24(data, 1),
            "short_mask": be_u24(data, 4), "counts_nibbles": data[7],
        }
    if can_id == 0x6AA:
        return {
            "segment": data[0], "jump_mask": be_u24(data, 1),
            "rate_mask": be_u24(data, 4), "counts_nibbles": data[7],
        }
    if can_id == 0x6AB:
        return {
            "segment": data[0], "voltage_jump_mask": be_u16(data, 1),
            "voltage_stuck_mask": be_u16(data, 3),
            "jump_count": data[5], "stuck_count": data[6], "flags": data[7],
        }
    if can_id == 0x6AD:
        return {
            "protocol_version": data[0], "logger_sequence": data[1],
            "phase": data[2], "phase_count": data[3],
            "measurement_sequence": be_u32(data, 4),
        }
    if can_id == 0x6AE:
        return {
            "i1_0p01A": valid_i16(be_i16(data, 0)),
            "i2_0p01A": valid_i16(be_i16(data, 2)),
            "v1_0p1V": valid_u16(be_u16(data, 4)),
            "v2_0p1V": valid_u16(be_u16(data, 6)),
        }
    if can_id == 0x6AF:
        return {
            "flags": data[0], "stage": data[1], "reason": data[2],
            "device_id": data[3], "conversion_count": be_u16(data, 4),
            "age_ms": be_u16(data, 6),
        }
    if can_id == 0x6B0:
        return {
            "i1_raw": be_i32(data, 0), "vb1_raw": be_i16(data, 4),
            "conversion_phase": data[6], "calibration_profile": data[7],
        }
    if can_id == 0x6B3:
        flags = data[7]
        return {
            "protected_deadline_miss": be_u16(data, 0),
            "detail_superseded": be_u16(data, 2),
            "detail_discarded_recovery": be_u16(data, 4),
            "protected_superseded": data[6],
            "tx_suspended": int(bool(flags & 0x01)),
            "tx_latched_inhibit": int(bool(flags & 0x02)),
            "busoff": int(bool(flags & 0x04)),
            "recover_pending": int(bool(flags & 0x08)),
            "flags": flags,
        }
    if can_id == 0x6B4:
        flags = data[1]
        return {
            "active_index": data[0],
            "raw_valid": int(bool(flags & 0x01)),
            "avg8_valid": int(bool(flags & 0x02)),
            "iir_valid": int(bool(flags & 0x04)),
            "selected_source": (flags >> 4) & 0x03,
            "avg8_minus_raw_mV": be_i16(data, 2),
            "iir_minus_raw_mV": be_i16(data, 4),
            "measurement_sequence_low16": be_u16(data, 6),
        }
    if can_id == 0x6B5:
        return {
            "segment": data[0], "tuning_sequence": data[1],
            "soc_0p01pct": valid_u16(be_u16(data, 2)),
            "vp1_mV": be_i16(data, 4), "vp2_mV": be_i16(data, 6),
        }
    if can_id == 0x6B6:
        return {
            "segment": data[0], "tuning_sequence": data[1],
            "measured_segment_mV": valid_u16(be_u16(data, 2)),
            "predicted_segment_mV": valid_u16(be_u16(data, 4)),
            "innovation_mV": be_i16(data, 6),
        }
    if can_id == 0x6B7:
        page = data[1] >> 6
        base = {"segment": data[0], "page": page,
                "tuning_sequence": data[1] & 0x3F}
        if page == 0:
            base.update({
                "p_soc_1e9": valid_u16(be_u16(data, 2)),
                "p_vp1_1e9": valid_u16(be_u16(data, 4)),
                "p_vp2_1e9": valid_u16(be_u16(data, 6)),
            })
        elif page == 1:
            base.update({
                "p_r0_1e12": valid_u16(be_u16(data, 2)),
                "measurement_r_1e9": valid_u16(be_u16(data, 4)),
                "dt_clamp_count_low16": be_u16(data, 6),
            })
        else:
            base.update({
                "innovation_reject_count_low16": be_u16(data, 2),
                "fault_flags_low16": be_u16(data, 4),
                "step_count_low16": be_u16(data, 6),
            })
        return base
    if can_id == 0x6B8:
        page = data[1] >> 7
        base = {"segment": data[0], "page": page,
                "tuning_sequence": data[1] & 0x7F}
        if page == 0:
            flags = data[7]
            base.update({
                "r0_uohm": valid_u16(be_u16(data, 2)),
                "resistance_growth_1e4": valid_u16(be_u16(data, 4)),
                "confidence_pct": data[6], "ekf_valid": int(bool(flags & 0x01)),
                "soh_status_low4": (flags >> 1) & 0x0F,
                "r0_update_rejected": int(bool(flags & 0x20)), "flags": flags,
            })
        else:
            base.update({
                "reference_r0_uohm": valid_u16(be_u16(data, 2)),
                "r0_variance_1e12": valid_u16(be_u16(data, 4)),
                "r0_reject_flags_low16": be_u16(data, 6),
            })
        return base
    if can_id == 0x6B9:
        page = data[1] >> 6
        base: dict[str, Any] = {
            "segment": data[0], "page": page,
            "tuning_sequence": data[1] & 0x3F,
        }
        if page == 0:
            base.update({
                "raw_segment_mV": valid_u16(be_u16(data, 2)),
                "avg8_segment_mV": valid_u16(be_u16(data, 4)),
                "iir_segment_mV": valid_u16(be_u16(data, 6)),
            })
        elif page == 1:
            base.update({
                "pack_current_0p1A": be_i16(data, 2),
                "surface_temp_0p1C": be_i16(data, 4),
                "core_temp_0p1C": be_i16(data, 6),
            })
        elif page == 2:
            base.update({
                "measurement_sequence_low16": be_u16(data, 2),
                "current_sequence_low16": be_u16(data, 4),
                "measurement_age_10ms": data[6], "current_age_10ms": data[7],
            })
        else:
            base.update({
                "fresh_temp_count": data[2], "model_domain_flags": data[3],
                "step_count_low16": be_u16(data, 4),
                "soh_accepted_sat8": data[6], "soh_rejected_sat8": data[7],
            })
        return base
    if can_id in (0x6BA, 0x6BB):
        prefix = "discharge" if can_id == 0x6BA else "charge"
        return {
            "horizon_index": data[0], "tuning_sequence": data[1],
            f"raw_model_{prefix}_0p1A": be_i16(data, 2),
            f"strategy_{prefix}_0p1A": be_i16(data, 4),
            f"final_{prefix}_0p1A": be_i16(data, 6),
        }
    if can_id == 0x6BC:
        page = data[0] >> 4
        base = {
            "page": page, "horizon_index": data[0] & 0x0F,
            "tuning_sequence": data[1],
        }
        if page == 0:
            base.update({
                "discharge_binding": data[2], "discharge_segment": data[3],
                "discharge_cell": data[4], "charge_binding": data[5],
                "charge_segment": data[6], "charge_cell": data[7],
            })
        else:
            base.update({
                "discharge_min_cell_mV": valid_u16(be_u16(data, 2)),
                "charge_max_cell_mV": valid_u16(be_u16(data, 4)),
                "discharge_power_0p1kW": be_i16(data, 6),
            })
        return base
    if can_id == 0x6BD:
        page = data[0]
        if page == 0:
            flags = data[1]
            return {
                "page": page, "fuse_valid": int(bool(flags & 0x01)),
                "fuse_authority_valid": int(bool(flags & 0x02)),
                "budget_exhausted": int(bool(flags & 0x04)),
                "utilization_1e4": valid_u16(be_u16(data, 2)),
                "estimated_temp_0p1C": be_i16(data, 4),
                "temperature_derating_1e4": valid_u16(be_u16(data, 6)),
            }
        if page == 1:
            return {
                "page": page, "reason_flags_low8": data[1],
                "effective_current_0p1A": be_i16(data, 2),
                "equivalent_25C_current_0p1A": be_i16(data, 4),
                "usable_melt_time_0p1s": valid_u16(be_u16(data, 6)),
            }
        return {
            "page": page, "tuning_sequence": data[1],
            "fuse_reason_flags_full16": be_u16(data, 2),
            "typical_melt_time_0p1s": valid_u16(be_u16(data, 4)),
            "usable_melt_time_0p1s": valid_u16(be_u16(data, 6)),
        }
    if can_id == 0x6BE:
        flags = data[1]
        return {
            "horizon_index": data[0], "fuse_valid": int(bool(flags & 0x01)),
            "fuse_authority_valid": int(bool(flags & 0x02)),
            "budget_exhausted": int(bool(flags & 0x04)),
            "fuse_cap_0p1A": valid_u16(be_u16(data, 2)),
            "hardware_cap_0p1A": valid_u16(be_u16(data, 4)),
            "sop_reason_flags_low16": be_u16(data, 6),
        }
    if can_id == 0x6BF:
        page = data[0]
        if page == 0:
            flags = data[3]
            return {
                "page": page, "tuning_sequence": data[1],
                "instance_count": data[2],
                "power_valid": int(bool(flags & 0x01)),
                "power_authority_valid": int(bool(flags & 0x02)),
                "estimator_step": be_u32(data, 4),
            }
        return {
            "page": page, "tuning_sequence": data[1],
            "source_tick_ms": be_u32(data, 2),
            "measurement_sequence_low16": be_u16(data, 6),
        }
    if can_id == 0x6C0:
        return {
            "horizon_index": data[0], "tuning_sequence": data[1],
            "charge_power_0p1kW": be_i16(data, 2),
            "sop_reason_flags_full32": be_u32(data, 4),
        }
    if can_id == 0x6F0:
        flags = data[3]
        return {
            "protocol_version": data[0],
            "last_protected_sequence": data[1],
            "last_detail_snapshot_sequence": data[2],
            "ams_fresh": int(bool(flags & 0x01)),
            "authority_valid": int(bool(flags & 0x02)),
            "detail_snapshot_complete": int(bool(flags & 0x04)),
            "ecu_raw_logger_overflow": int(bool(flags & 0x08)),
            "flags": flags,
            "rx_drop_diag": be_u16(data, 4),
            "uptime_counter": be_u16(data, 6),
        }
    return {}


def iter_records(raw: bytes) -> Iterable[tuple[int, int, int, int, int, bytes]]:
    payload_bytes = len(raw) - HEADER_SIZE
    complete_records = payload_bytes // RECORD.size
    for index in range(complete_records):
        off = HEADER_SIZE + index * RECORD.size
        ts, seq, can_id, dlc, flags, data, _ = RECORD.unpack_from(raw, off)
        yield ts, seq, can_id, dlc, flags, data


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("-o", "--output", type=Path)
    parser.add_argument(
        "--no-derived", action="store_true",
        help="only write the one-row-per-CAN-frame CSV"
    )
    args = parser.parse_args()

    output = args.output or args.input.with_suffix(".csv")
    raw = args.input.read_bytes()
    if len(raw) < HEADER_SIZE or raw[:7] != b"DERCAN1":
        raise SystemExit("not a DER26 raw CAN log")

    schema = raw[8]
    record_size = int.from_bytes(raw[10:12], "little")
    if record_size != RECORD.size:
        raise SystemExit(f"unsupported record size {record_size}")

    payload_bytes = len(raw) - HEADER_SIZE
    complete_records, trailing = divmod(payload_bytes, RECORD.size)
    records = list(iter_records(raw))

    with output.open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow([
            "timestamp_ms", "sequence", "id_hex", "id_name", "dlc",
            "standard", "remote", "known_ams", "known_cm200", "parsed",
            "payload_hex", "decoded_fields_json", *[f"data{i}" for i in range(8)]
        ])
        for ts, seq, can_id, dlc, flags, data in records:
            valid_dlc = min(dlc, 8)
            fields = decode_fields(can_id, data)
            writer.writerow([
                ts, seq, f"0x{can_id:X}", ID_NAMES.get(can_id, ""), dlc,
                int(bool(flags & 1)), int(bool(flags & 2)),
                int(bool(flags & 4)), int(bool(flags & 8)),
                int(bool(flags & 16)), data[:valid_dlc].hex().upper(),
                json.dumps(fields, sort_keys=True, separators=(",", ":")) if fields else "",
                *data,
            ])

    derived_paths: list[Path] = []
    if not args.no_derived:
        stem = output.with_suffix("")
        cell_path = stem.parent / f"{stem.name}_ams_cells.csv"
        temp_path = stem.parent / f"{stem.name}_ams_temps.csv"
        snapshot_path = stem.parent / f"{stem.name}_ams_snapshots.csv"
        apm_path = stem.parent / f"{stem.name}_ams_apm.csv"
        ekf_path = stem.parent / f"{stem.name}_ams_ekf.csv"
        cov_path = stem.parent / f"{stem.name}_ams_ekf_cov.csv"
        soh_path = stem.parent / f"{stem.name}_ams_soh.csv"
        sop_path = stem.parent / f"{stem.name}_ams_sop.csv"
        fuse_path = stem.parent / f"{stem.name}_ams_fuse.csv"
        context = SnapshotContext()

        with (
            cell_path.open("w", newline="") as cell_stream,
            temp_path.open("w", newline="") as temp_stream,
            snapshot_path.open("w", newline="") as snapshot_stream,
            apm_path.open("w", newline="") as apm_stream,
            ekf_path.open("w", newline="") as ekf_stream,
            cov_path.open("w", newline="") as cov_stream,
            soh_path.open("w", newline="") as soh_stream,
            sop_path.open("w", newline="") as sop_stream,
            fuse_path.open("w", newline="") as fuse_stream,
        ):
            cell_writer = csv.writer(cell_stream)
            temp_writer = csv.writer(temp_stream)
            snap_writer = csv.writer(snapshot_stream)
            apm_writer = csv.writer(apm_stream)
            common_tuning = [
                "timestamp_ms", "can_sequence", "frame", "segment", "page",
                "horizon_index", "tuning_sequence",
            ]
            ekf_fields = common_tuning + [
                "soc_0p01pct", "vp1_mV", "vp2_mV",
                "measured_segment_mV", "predicted_segment_mV", "innovation_mV",
                "raw_segment_mV", "avg8_segment_mV", "iir_segment_mV",
                "pack_current_0p1A", "surface_temp_0p1C", "core_temp_0p1C",
                "measurement_sequence_low16", "current_sequence_low16",
                "measurement_age_10ms", "current_age_10ms", "fresh_temp_count",
                "model_domain_flags", "step_count_low16", "soh_accepted_sat8",
                "soh_rejected_sat8", "instance_count", "power_valid",
                "power_authority_valid", "estimator_step", "source_tick_ms",
            ]
            cov_fields = common_tuning + [
                "p_soc_1e9", "p_vp1_1e9", "p_vp2_1e9", "p_r0_1e12",
                "measurement_r_1e9", "dt_clamp_count_low16",
                "innovation_reject_count_low16", "fault_flags_low16",
                "step_count_low16",
            ]
            soh_fields = common_tuning + [
                "r0_uohm", "resistance_growth_1e4", "confidence_pct",
                "ekf_valid", "soh_status_low4", "r0_update_rejected", "flags",
                "reference_r0_uohm", "r0_variance_1e12",
                "r0_reject_flags_low16",
            ]
            sop_fields = common_tuning + [
                "raw_model_discharge_0p1A", "strategy_discharge_0p1A",
                "final_discharge_0p1A", "raw_model_charge_0p1A",
                "strategy_charge_0p1A", "final_charge_0p1A",
                "discharge_binding", "discharge_segment", "discharge_cell",
                "charge_binding", "charge_segment", "charge_cell",
                "discharge_min_cell_mV", "charge_max_cell_mV",
                "discharge_power_0p1kW", "charge_power_0p1kW",
                "sop_reason_flags_full32",
            ]
            fuse_fields = common_tuning + [
                "fuse_valid", "fuse_authority_valid", "budget_exhausted",
                "utilization_1e4", "estimated_temp_0p1C",
                "temperature_derating_1e4", "reason_flags_low8",
                "effective_current_0p1A", "equivalent_25C_current_0p1A",
                "usable_melt_time_0p1s", "fuse_cap_0p1A",
                "hardware_cap_0p1A", "sop_reason_flags_low16",
                "fuse_reason_flags_full16", "typical_melt_time_0p1s",
            ]
            ekf_writer = csv.DictWriter(ekf_stream, fieldnames=ekf_fields,
                                        extrasaction="ignore")
            cov_writer = csv.DictWriter(cov_stream, fieldnames=cov_fields,
                                        extrasaction="ignore")
            soh_writer = csv.DictWriter(soh_stream, fieldnames=soh_fields,
                                        extrasaction="ignore")
            sop_writer = csv.DictWriter(sop_stream, fieldnames=sop_fields,
                                        extrasaction="ignore")
            fuse_writer = csv.DictWriter(fuse_stream, fieldnames=fuse_fields,
                                         extrasaction="ignore")
            for writer in (ekf_writer, cov_writer, soh_writer, sop_writer, fuse_writer):
                writer.writeheader()
            context_header = [
                "logger_protocol", "logger_sequence", "phase", "phase_count",
                "measurement_sequence",
            ]
            cell_writer.writerow([
                "timestamp_ms", "can_sequence", *context_header,
                "segment", "cell_index_zero_based", "cell_number", "millivolts", "valid",
            ])
            temp_writer.writerow([
                "timestamp_ms", "can_sequence", *context_header,
                "segment", "sensor_index_zero_based", "temp_0p1C", "valid",
            ])
            snap_writer.writerow([
                "timestamp_ms", "can_sequence", *context_header,
            ])
            apm_writer.writerow([
                "timestamp_ms", "can_sequence", *context_header,
                "frame", "i1_0p01A", "i2_0p01A", "v1_0p1V", "v2_0p1V",
                "flags", "stage", "reason", "device_id", "conversion_count",
                "age_ms", "i1_raw", "vb1_raw", "conversion_phase",
                "calibration_profile",
            ])

            def ctx() -> list[int | None]:
                return [
                    context.version, context.logger_sequence, context.phase,
                    context.phase_count, context.measurement_sequence,
                ]

            for ts, seq, can_id, _dlc, _flags, data in records:
                if can_id == 0x6AD:
                    context = SnapshotContext(
                        version=data[0], logger_sequence=data[1], phase=data[2],
                        phase_count=data[3], measurement_sequence=be_u32(data, 4),
                    )
                    snap_writer.writerow([ts, seq, *ctx()])
                elif can_id == 0x6A0:
                    segment, start = data[0], data[1]
                    values = [be_u16(data, 2), be_u16(data, 4), be_u16(data, 6)]
                    for offset, raw_value in enumerate(values):
                        value = valid_u16(raw_value)
                        index = start + offset
                        cell_writer.writerow([
                            ts, seq, *ctx(), segment, index, index + 1,
                            "" if value is None else value, int(value is not None),
                        ])
                elif can_id == 0x6A1:
                    segment, start = data[0], data[1]
                    values = [be_i16(data, 2), be_i16(data, 4), be_i16(data, 6)]
                    for offset, raw_value in enumerate(values):
                        value = valid_i16(raw_value)
                        temp_writer.writerow([
                            ts, seq, *ctx(), segment, start + offset,
                            "" if value is None else value, int(value is not None),
                        ])
                elif can_id in (0x6AE, 0x6AF, 0x6B0):
                    values: dict[str, Any] = decode_fields(can_id, data)
                    apm_writer.writerow([
                        ts, seq, *ctx(), ID_NAMES[can_id],
                        values.get("i1_0p01A", ""), values.get("i2_0p01A", ""),
                        values.get("v1_0p1V", ""), values.get("v2_0p1V", ""),
                        values.get("flags", ""), values.get("stage", ""),
                        values.get("reason", ""), values.get("device_id", ""),
                        values.get("conversion_count", ""), values.get("age_ms", ""),
                        values.get("i1_raw", ""), values.get("vb1_raw", ""),
                        values.get("conversion_phase", ""),
                        values.get("calibration_profile", ""),
                    ])
                elif 0x6B5 <= can_id <= 0x6C0:
                    values = decode_fields(can_id, data)
                    row = {
                        "timestamp_ms": ts, "can_sequence": seq,
                        "frame": ID_NAMES[can_id], **values,
                    }
                    if can_id in (0x6B5, 0x6B6, 0x6B9, 0x6BF):
                        ekf_writer.writerow(row)
                    elif can_id == 0x6B7:
                        cov_writer.writerow(row)
                    elif can_id == 0x6B8:
                        soh_writer.writerow(row)
                    elif can_id in (0x6BA, 0x6BB, 0x6BC, 0x6C0):
                        sop_writer.writerow(row)
                    else:
                        fuse_writer.writerow(row)

        derived_paths.extend([
            cell_path, temp_path, ekf_path, cov_path, soh_path, sop_path,
            fuse_path, snapshot_path, apm_path,
        ])

    print(f"decoded schema={schema} records={complete_records} trailing_bytes={trailing}")
    print(output)
    for path in derived_paths:
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
