#!/usr/bin/env python3
"""Self-contained regression test for Tools/decode_can_log.py."""
from __future__ import annotations

import csv
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
DECODER = ROOT / "Tools/decode_can_log.py"
RECORD = struct.Struct("<IIIBB8sH")


def rec(ts: int, seq: int, can_id: int, payload: bytes) -> bytes:
    payload = payload.ljust(8, b"\x00")[:8]
    flags = 1 | 4 | 16  # standard, known AMS, parsed
    return RECORD.pack(ts, seq, can_id, 8, flags, payload, 0)


header = bytearray(32)
header[:7] = b"DERCAN1"
header[8] = 1
header[10:12] = RECORD.size.to_bytes(2, "little")
header[12:15] = bytes((2, 8, 0))
raw = bytes(header)
raw += rec(100, 1, 0x6AD, bytes((1, 9, 0, 1)) + (1234).to_bytes(4, "big"))
raw += rec(101, 2, 0x6A0, bytes((0, 0)) + (3500).to_bytes(2, "big") + (3501).to_bytes(2, "big") + (0xFFFF).to_bytes(2, "big"))
raw += rec(102, 3, 0x6A1, bytes((0, 0)) + (225).to_bytes(2, "big", signed=True) + (-50).to_bytes(2, "big", signed=True) + (-32768).to_bytes(2, "big", signed=True))
raw += rec(103, 4, 0x6AE, (125).to_bytes(2, "big", signed=True) + (-120).to_bytes(2, "big", signed=True) + (120).to_bytes(2, "big") + (121).to_bytes(2, "big"))
raw += rec(104, 5, 0x6AF, bytes((0x7F, 4, 0, 6)) + (55).to_bytes(2, "big") + (20).to_bytes(2, "big"))
raw += rec(105, 6, 0x6B0, (-123456).to_bytes(4, "big", signed=True) + (-321).to_bytes(2, "big", signed=True) + bytes((2, 1)))
raw += rec(106, 7, 0x6B2, bytes((0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08)))
raw += rec(107, 8, 0x6B3, (3).to_bytes(2, "big") + (9).to_bytes(2, "big") + (2).to_bytes(2, "big") + bytes((4, 0x0B)))
raw += rec(108, 9, 0x6B4, bytes((2, 0x17)) + (12).to_bytes(2, "big", signed=True) + (-25).to_bytes(2, "big", signed=True) + (0x5678).to_bytes(2, "big"))
raw += rec(109, 10, 0x6F0, bytes((4, 0x22, 0x33, 0x0F)) + (7).to_bytes(2, "big") + (99).to_bytes(2, "big"))
raw += rec(110, 11, 0x6B5, bytes((3, 17)) + (8123).to_bytes(2, "big") + (45).to_bytes(2, "big", signed=True) + (-12).to_bytes(2, "big", signed=True))
raw += rec(111, 12, 0x6B6, bytes((3, 17)) + (55400).to_bytes(2, "big") + (55380).to_bytes(2, "big") + (20).to_bytes(2, "big", signed=True))
raw += rec(112, 13, 0x6B7, bytes((3, 17)) + (100).to_bytes(2, "big") + (200).to_bytes(2, "big") + (300).to_bytes(2, "big"))
raw += rec(113, 14, 0x6B8, bytes((3, 17)) + (4200).to_bytes(2, "big") + (11500).to_bytes(2, "big") + bytes((87, 0x2B)))
raw += rec(114, 15, 0x6B9, bytes((3, 0x80 | 17)) + (222).to_bytes(2, "big") + (333).to_bytes(2, "big") + bytes((4, 2)))
raw += rec(115, 16, 0x6BA, bytes((2, 17)) + (760).to_bytes(2, "big", signed=True) + (700).to_bytes(2, "big", signed=True) + (650).to_bytes(2, "big", signed=True))
raw += rec(116, 17, 0x6BB, bytes((2, 17)) + (-400).to_bytes(2, "big", signed=True) + (-350).to_bytes(2, "big", signed=True) + (-300).to_bytes(2, "big", signed=True))
raw += rec(117, 18, 0x6BC, bytes((2, 17, 1, 3, 14, 2, 4, 0)))
raw += rec(118, 19, 0x6BD, bytes((0, 3)) + (4321).to_bytes(2, "big") + (650).to_bytes(2, "big", signed=True) + (9000).to_bytes(2, "big"))
raw += rec(119, 20, 0x6BE, bytes((2, 3)) + (720).to_bytes(2, "big") + (800).to_bytes(2, "big") + (0x1234).to_bytes(2, "big"))
raw += rec(120, 21, 0x6B7, bytes((3, 0x40 | 17)) + (400).to_bytes(2, "big") + (500).to_bytes(2, "big") + (6).to_bytes(2, "big"))
raw += rec(121, 22, 0x6B7, bytes((3, 0x80 | 17)) + (7).to_bytes(2, "big") + (8).to_bytes(2, "big") + (900).to_bytes(2, "big"))
raw += rec(122, 23, 0x6B8, bytes((3, 0x80 | 17)) + (4100).to_bytes(2, "big") + (12).to_bytes(2, "big") + (0x0200).to_bytes(2, "big"))

with tempfile.TemporaryDirectory() as td:
    directory = Path(td)
    binary = directory / "CAN000.BIN"
    output = directory / "decoded.csv"
    binary.write_bytes(raw)
    subprocess.run([sys.executable, str(DECODER), str(binary), "-o", str(output)], check=True, capture_output=True, text=True)

    with output.open(newline="") as f:
        rows = list(csv.DictReader(f))
    assert len(rows) == 23
    assert rows[1]["id_name"] == "AMS_LOG_CELL_DETAIL"
    assert '"cell0_mV":3500' in rows[1]["decoded_fields_json"]
    assert rows[6]["id_name"] == "AMS_ESTIMATOR_V4"
    assert rows[7]["id_name"] == "AMS_LOG_TX_SCHEDULER"
    assert '"protected_deadline_miss":3' in rows[7]["decoded_fields_json"]
    assert '"tx_latched_inhibit":1' in rows[7]["decoded_fields_json"]
    assert rows[8]["id_name"] == "AMS_ESTIMATOR_VOLTAGE_COMPARE"
    assert '"avg8_minus_raw_mV":12' in rows[8]["decoded_fields_json"]
    assert '"iir_minus_raw_mV":-25' in rows[8]["decoded_fields_json"]
    assert rows[9]["id_name"] == "ECU_AMS_DIAG_FEEDBACK"
    assert '"authority_valid":1' in rows[9]["decoded_fields_json"]
    assert '"uptime_counter":99' in rows[9]["decoded_fields_json"]
    assert rows[10]["id_name"] == "AMS_TUNING_EKF_STATE"
    assert '"soc_0p01pct":8123' in rows[10]["decoded_fields_json"]
    assert rows[19]["id_name"] == "AMS_TUNING_FUSE_LIMIT"
    assert '"sop_reason_flags_low16":4660' in rows[19]["decoded_fields_json"]
    assert '"measurement_r_1e9":500' in rows[20]["decoded_fields_json"]
    assert '"fault_flags_low16":8' in rows[21]["decoded_fields_json"]
    assert '"reference_r0_uohm":4100' in rows[22]["decoded_fields_json"]

    with (directory / "decoded_ams_cells.csv").open(newline="") as f:
        cells = list(csv.DictReader(f))
    assert len(cells) == 3
    assert cells[0]["logger_sequence"] == "9"
    assert cells[0]["measurement_sequence"] == "1234"
    assert cells[0]["millivolts"] == "3500"
    assert cells[2]["valid"] == "0"

    with (directory / "decoded_ams_temps.csv").open(newline="") as f:
        temps = list(csv.DictReader(f))
    assert temps[0]["temp_0p1C"] == "225"
    assert temps[1]["temp_0p1C"] == "-50"
    assert temps[2]["valid"] == "0"

    with (directory / "decoded_ams_apm.csv").open(newline="") as f:
        apm = list(csv.DictReader(f))
    assert len(apm) == 3
    assert apm[0]["i1_0p01A"] == "125"
    assert apm[1]["device_id"] == "6"
    assert apm[2]["i1_raw"] == "-123456"

    with (directory / "decoded_ams_ekf.csv").open(newline="") as f:
        ekf = list(csv.DictReader(f))
    assert len(ekf) == 3
    assert ekf[0]["segment"] == "3" and ekf[0]["vp1_mV"] == "45"
    assert ekf[2]["measurement_sequence_low16"] == "222"

    with (directory / "decoded_ams_ekf_cov.csv").open(newline="") as f:
        cov = list(csv.DictReader(f))
    assert len(cov) == 3 and cov[0]["p_vp2_1e9"] == "300"
    assert cov[1]["measurement_r_1e9"] == "500"
    assert cov[2]["innovation_reject_count_low16"] == "7"

    with (directory / "decoded_ams_soh.csv").open(newline="") as f:
        soh = list(csv.DictReader(f))
    assert len(soh) == 2
    assert soh[0]["r0_uohm"] == "4200" and soh[0]["confidence_pct"] == "87"
    assert soh[1]["reference_r0_uohm"] == "4100"

    with (directory / "decoded_ams_sop.csv").open(newline="") as f:
        sop = list(csv.DictReader(f))
    assert len(sop) == 3
    assert sop[0]["final_discharge_0p1A"] == "650"
    assert sop[1]["final_charge_0p1A"] == "-300"

    with (directory / "decoded_ams_fuse.csv").open(newline="") as f:
        fuse = list(csv.DictReader(f))
    assert len(fuse) == 2
    assert fuse[0]["utilization_1e4"] == "4321"
    assert fuse[1]["hardware_cap_0p1A"] == "800"

print("PASS raw CAN decoder and derived AMS tables")
