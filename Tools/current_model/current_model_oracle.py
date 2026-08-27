#!/usr/bin/env python3
"""Independent Python oracle for the synthetic ECU current-model fixture.

The implementation intentionally does not import, execute, or bind to the C
model. It reproduces the frozen interval semantics used by the differential
campaign: outward uncertainty, every touched torque cell, operating-domain
intersection, auxiliary current once, numerical outward margin, and smallest
qualifying monotonic transition span.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable


AXIS = (-100.0, 0.0, 50.0, 100.0, 200.0)
STEADY = ((-20.0, 10.0), (0.0, 25.0), (10.0, 55.0), (20.0, 110.0))
AUX = (0.0, 0.25)
NUMERIC = (0.001, 0.001)
MICRO = (0.1, 0.2)
TORQUE_UNCERTAINTY = (0.2, 0.3)
MAX_AGE = (100_000, 100_000, 200_000, 200_000)
PROFILE_SLEW = 3
DIRECTION_FROM_ZERO = 5
STATUS_OK = 1
STATUS_REGION_OVERFLOW = 3
STATUS_OUT_OF_DOMAIN = 2
MAX_TORQUE_CELLS = 4
TRANSITIONS = {
    50.0: (-20.0, 45.0, 45_000),
    100.0: (-25.0, 70.0, 70_000),
    200.0: (-35.0, 120.0, 120_000),
}


def overlaps(a_min: float, a_max: float, b_min: float, b_max: float) -> bool:
    return a_max >= b_min and b_max >= a_min


def steady(torque: float, speed: float, vdc: float, inverter_temp: float,
           motor_temp: float, speed_age: int, vdc_age: int,
           inverter_temp_age: int, motor_temp_age: int,
           microstep: bool) -> tuple[int, float, float, int, int]:
    if (speed_age > MAX_AGE[0] or vdc_age > MAX_AGE[1] or
            inverter_temp_age > MAX_AGE[2] or
            motor_temp_age > MAX_AGE[3]):
        return STATUS_OUT_OF_DOMAIN, 0.0, 0.0, 0, 0
    torque_min = torque - TORQUE_UNCERTAINTY[0]
    torque_max = torque + TORQUE_UNCERTAINTY[1]
    touched = [i for i in range(len(STEADY))
               if overlaps(torque_min, torque_max, AXIS[i], AXIS[i + 1])]
    if not touched:
        return STATUS_OUT_OF_DOMAIN, 0.0, 0.0, 0, 0
    if len(touched) > MAX_TORQUE_CELLS:
        return STATUS_REGION_OVERFLOW, 0.0, 0.0, 0, 0
    # The fixture has one operating region spanning every generated input.
    minimum = min(STEADY[i][0] for i in touched) + AUX[0] - NUMERIC[0]
    maximum = max(STEADY[i][1] for i in touched) + AUX[1] + NUMERIC[1]
    if microstep:
        minimum -= MICRO[0]
        maximum += MICRO[1]
    return STATUS_OK, minimum, maximum, len(touched), len(touched)


def transition(span: float, profile: int, direction: int) -> tuple[int, float, float, int, int]:
    if profile != PROFILE_SLEW or direction != DIRECTION_FROM_ZERO:
        return STATUS_OUT_OF_DOMAIN, 0.0, 0.0, 0, 0
    qualifying = [limit for limit in TRANSITIONS if span <= limit]
    if not qualifying:
        return STATUS_OUT_OF_DOMAIN, 0.0, 0.0, 0, 0
    limit = min(qualifying)
    minimum, maximum, settling = TRANSITIONS[limit]
    return STATUS_OK, minimum - NUMERIC[0], maximum + NUMERIC[1], settling, 1
