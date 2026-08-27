#!/usr/bin/env python3
"""Verify the validated DER26 F767 CAN timing/clock contract."""
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()

def r(rel):
    p=root/rel
    if not p.exists():
        raise SystemExit(f"FAIL missing {rel}")
    return p.read_text(errors="replace")

app=r("Core/Inc/app.h")
main=r("Core/Src/main.c")
ioc=r("DER26-ECU.ioc")

checks = {
    "default vehicle bitrate 500 kbps": "#define DER26_CAN_BITRATE_KBPS 500u" in app,
    "500k prescaler 6": "#define DER26_CAN_PRESCALER 6u" in app,
    "250k prescaler 12": "#define DER26_CAN_PRESCALER 12u" in app,
    "SJW 2 TQ": "#define DER26_CAN_SJW CAN_SJW_2TQ" in app,
    "BS1 15 TQ": "#define DER26_CAN_BS1 CAN_BS1_15TQ" in app,
    "BS2 2 TQ": "#define DER26_CAN_BS2 CAN_BS2_2TQ" in app,
    "main prescaler macro": "hcan1.Init.Prescaler = DER26_CAN_PRESCALER" in main,
    "main SJW macro": "hcan1.Init.SyncJumpWidth = DER26_CAN_SJW" in main,
    "main BS1 macro": "hcan1.Init.TimeSeg1 = DER26_CAN_BS1" in main,
    "main BS2 macro": "hcan1.Init.TimeSeg2 = DER26_CAN_BS2" in main,
    "PLL sourced from HSE": "RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE" in main,
    "IOC HSE source": "RCC.PLLSourceVirtual=RCC_PLLSOURCE_HSE" in ioc,
    "IOC 500k prescaler": "CAN1.Prescaler=6" in ioc,
    "IOC SJW2": "CAN1.SJW=CAN_SJW_2TQ" in ioc,
    "IOC 500k computed rate": "CAN1.CalculateBaudRate=500000" in ioc,
}
failed=[name for name,ok in checks.items() if not ok]
if failed:
    raise SystemExit("FAIL CAN clock contract: " + ", ".join(failed))

print("PASS ECU 54-MHz/HSE-derived 500k/250k CAN timing contract (SJW=2)")
