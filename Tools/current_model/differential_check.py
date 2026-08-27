#!/usr/bin/env python3
from __future__ import annotations

import csv
import math
import subprocess
import sys
from pathlib import Path

from current_model_oracle import steady, transition


def close(a: float, b: float, tolerance: float = 2e-4) -> bool:
    return math.isclose(a, b, rel_tol=0.0, abs_tol=tolerance)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: differential_check.py <vector-dump-binary>", file=sys.stderr)
        return 2
    binary = Path(sys.argv[1])
    output = subprocess.check_output([str(binary)], text=True)
    checked = 0
    for row in csv.reader(output.splitlines()):
        if row[0] == "S":
            torque, speed, vdc, inverter_temp, motor_temp = map(float, row[1:6])
            speed_age, vdc_age, inverter_temp_age, motor_temp_age, micro = \
                map(int, row[6:11])
            actual = (int(row[11]), float(row[12]), float(row[13]),
                      int(row[14]), int(row[15]))
            expected = steady(torque, speed, vdc, inverter_temp, motor_temp,
                              speed_age, vdc_age, inverter_temp_age,
                              motor_temp_age, bool(micro))
        elif row[0] == "T":
            span = float(row[1])
            profile, direction = int(row[2]), int(row[3])
            actual = (int(row[4]), float(row[5]), float(row[6]),
                      int(row[7]), int(row[8]))
            expected = transition(span, profile, direction)
        else:
            raise AssertionError(f"unknown row: {row}")
        assert actual[0] == expected[0], (row, actual, expected)
        assert close(actual[1], expected[1]), (row, actual, expected)
        assert close(actual[2], expected[2]), (row, actual, expected)
        assert actual[3:] == expected[3:], (row, actual, expected)
        checked += 1
    print(f"PASS Python/C current-model differential vectors: {checked}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
