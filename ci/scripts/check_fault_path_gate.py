#!/usr/bin/env python3
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
checks = [
    (root / "Core/Src/stm32f7xx_it.c", [
        "NMI_Handler", "HardFault_Handler", "MemManage_Handler",
        "BusFault_Handler", "UsageFault_Handler"
    ]),
    (root / "Core/Src/main.c", ["Error_Handler"]),
]
errors = []

# The global panic primitive itself is part of the safety contract.  It must
# both drop propulsion/control outputs and place the coolant PWM node into the
# electrically verified no-valid-PWM/full-speed fallback without relying on
# the scheduler or HAL.  Keep this as a source gate because host tests do not
# model STM32 timer/GPIO register side effects.
app_text = (root / "Core/Src/app.c").read_text()
m = re.search(r"void\s+ecu_force_safe_outputs\s*\([^)]*\)\s*\{", app_text)
if not m:
    errors.append("Core/Src/app.c: missing ecu_force_safe_outputs")
else:
    start = m.end()
    next_fn = re.search(r"\nvoid\s+[A-Za-z_]\w*\s*\(", app_text[start:])
    end = start + (next_fn.start() if next_fn else len(app_text[start:]))
    body = app_text[start:end]
    required = {
        "Firmware_OK/Cascadia fail-low latch": "Firmware_Ok_Pin | Cascadia_ON_Pin",
        "motor-enable/buzzer fail-low latch": "MTR_EN_Pin | Buzzer_Pin",
        "TIM4 CH3 disabled": "TIM4->CCER &= ~TIM_CCER_CC3E",
        "all safety GPIO clocks enabled": "RCC_AHB1ENR_GPIOAEN |",
        "GPIOB safety clock enabled": "RCC_AHB1ENR_GPIOBEN |",
        "GPIOF safety clock enabled": "RCC_AHB1ENR_GPIOFEN",
        "GPIOA safety pins forced output": "GPIOA->MODER",
        "GPIOF safety pins forced output": "GPIOF->MODER",
        "PB8 forced GPIO output": "GPIOB->MODER",
        "PB8 pump gate forced low": "GPIOB->BSRR = ((uint32_t)GPIO_PIN_8 << 16u)",
    }
    for label, token in required.items():
        if token not in body:
            errors.append(f"Core/Src/app.c: ecu_force_safe_outputs missing {label}")

# Compile-time inhibited images must also fail low at the final hardware
# writer, not only at callers in the error/CAN policy tasks.
for setter in ("set_ecu_ok", "set_cascadia_enable", "set_cascadia_on"):
    m = re.search(rf"void\s+{setter}\s*\([^)]*\)\s*\{{", app_text)
    if not m:
        errors.append(f"Core/Src/app.c: missing {setter}")
        continue
    start = m.end()
    next_fn = re.search(r"\nvoid\s+[A-Za-z_]\w*\s*\(", app_text[start:])
    end = start + (next_fn.start() if next_fn else len(app_text[start:]))
    body = app_text[start:end]
    if "#if ECU_OUTPUTS_INHIBITED" not in body or "state = false;" not in body:
        errors.append(f"Core/Src/app.c:{setter} lacks final compile-time fail-low interlock")

for path, names in checks:
    text = path.read_text()
    for name in names:
        m = re.search(rf"void\s+{re.escape(name)}\s*\([^)]*\)\s*\{{", text)
        if not m:
            errors.append(f"{path.relative_to(root)}: missing {name}")
            continue
        start = m.end()
        next_fn = re.search(r"\nvoid\s+[A-Za-z_]\w*\s*\(", text[start:])
        end = start + (next_fn.start() if next_fn else len(text[start:]))
        body = text[start:end]
        safe = body.find("ecu_force_safe_outputs();")
        loop = body.find("while (1)")
        if safe < 0:
            errors.append(f"{path.relative_to(root)}:{name} does not force safe outputs")
        elif loop >= 0 and safe > loop:
            errors.append(f"{path.relative_to(root)}:{name} forces safe outputs after terminal loop")
if errors:
    print("ERROR critical fault path gate")
    print("\n".join(errors))
    sys.exit(1)
print("PASS panic primitive fail-lows propulsion + coolant PWM; NMI/CPU faults/Error_Handler invoke it before trapping")
