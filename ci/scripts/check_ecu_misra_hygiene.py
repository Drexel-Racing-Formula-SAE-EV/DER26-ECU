#!/usr/bin/env python3
"""ECU MISRA-lite hygiene gate.

This is not a certified MISRA checker. It is a CI ratchet for first-party ECU
firmware files: it blocks high-signal hygiene violations that make future MISRA C
migration harder, while reporting lower-confidence items as warnings.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

FIRST_PARTY_GLOBS = (
    "Core/Inc/app.h",
    "Core/Inc/board.h",
    "Core/Inc/ext_drivers/*.h",
    "Core/Inc/tasks/*.h",
    "Core/Src/app.c",
    "Core/Src/board.c",
    "Core/Src/stm32f7xx_it.c",
    "Core/Src/ext_drivers/*.c",
    "Core/Src/tasks/*.c",
)

EXCLUDED_FILES = {
    # CubeMX / vendor / generated code is intentionally excluded from this
    # first-party MISRA-lite gate.
    "Core/Inc/main.h",
    "Core/Inc/stm32f7xx_hal_conf.h",
    "Core/Inc/stm32f7xx_it.h",
    "Core/Inc/FreeRTOSConfig.h",
    "Core/Src/main.c",
    "Core/Src/freertos.c",
    "Core/Src/system_stm32f7xx.c",
    "Core/Src/stm32f7xx_hal_msp.c",
    "Core/Src/stm32f7xx_hal_timebase_tim.c",
    "Core/Src/syscalls.c",
    "Core/Src/sysmem.c",
}

BIDI_CHARS = {
    "\u202A", "\u202B", "\u202C", "\u202D", "\u202E",
    "\u2066", "\u2067", "\u2068", "\u2069",
}

ERROR_PATTERNS = (
    ("dynamic memory allocation", re.compile(r"\b(malloc|calloc|realloc|free)\s*\(")),
    ("goto statement", re.compile(r"\bgoto\b")),
    ("unsafe string function", re.compile(r"\b(gets|strcpy|strcat|sprintf)\s*\(")),
    ("malformed float suffix", re.compile(r"(?<![A-Za-z0-9_])\d+\.\d+f{2,}(?![A-Za-z0-9_])")),
)

WARNING_PATTERNS = (
    ("atoi without error handling", re.compile(r"\batoi\s*\(")),
    ("status accumulation with |=", re.compile(r"\|=")),
    ("function-like macro", re.compile(r"^\s*#\s*define\s+[A-Za-z_][A-Za-z0-9_]*\(", re.MULTILINE)),
    ("unsuffixed decimal floating literal", re.compile(r"(?<![A-Za-z0-9_])\d+\.\d+(?![A-Za-z0-9_])")),
)

HEADER_GUARD_RE = re.compile(r"^\s*#\s*(ifndef|define)\s+(__[A-Za-z0-9_]*|_[A-Z][A-Za-z0-9_]*)", re.MULTILINE)
EMPTY_PARAMS_RE = re.compile(
    r"^\s*(?:static\s+)?[A-Za-z_][A-Za-z0-9_\s\*]*\s+[A-Za-z_][A-Za-z0-9_]*\s*\(\s*\)\s*(?:;|\{)",
    re.MULTILINE,
)
PUBLIC_TASK_FN_RE = re.compile(r"^\s*void\s+[A-Za-z0-9_]+_task_fn\s*\(\s*void\s*\*\s*arg\s*\)", re.MULTILINE)


def strip_c_comments(text: str) -> str:
    """Return text with C/C++ comments replaced by whitespace, preserving lines."""
    result: list[str] = []
    i = 0
    in_block = False
    in_line = False
    in_string = False
    in_char = False
    escape = False

    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if (i + 1) < len(text) else ""

        if in_line:
            if ch == "\n":
                in_line = False
                result.append(ch)
            else:
                result.append(" ")
            i += 1
            continue

        if in_block:
            if (ch == "*") and (nxt == "/"):
                result.append("  ")
                i += 2
                in_block = False
            else:
                result.append("\n" if ch == "\n" else " ")
                i += 1
            continue

        if in_string or in_char:
            result.append(ch)
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif in_string and ch == '"':
                in_string = False
            elif in_char and ch == "'":
                in_char = False
            i += 1
            continue

        if (ch == "/") and (nxt == "/"):
            result.append("  ")
            i += 2
            in_line = True
            continue

        if (ch == "/") and (nxt == "*"):
            result.append("  ")
            i += 2
            in_block = True
            continue

        if ch == '"':
            in_string = True
        elif ch == "'":
            in_char = True

        result.append(ch)
        i += 1

    return "".join(result)


def iter_first_party_files() -> list[Path]:
    files: set[Path] = set()
    for pattern in FIRST_PARTY_GLOBS:
        files.update(ROOT.glob(pattern))
    return sorted(p for p in files if p.is_file() and p.relative_to(ROOT).as_posix() not in EXCLUDED_FILES)


def line_for_offset(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def add_match(messages: list[str], rel: str, label: str, text: str, match: re.Match[str]) -> None:
    messages.append(f"{rel}:{line_for_offset(text, match.start())}: {label}")


def main() -> int:
    errors: list[str] = []
    warnings: list[str] = []

    for path in iter_first_party_files():
        rel = path.relative_to(ROOT).as_posix()
        raw = path.read_bytes()

        if raw.startswith(b"\xef\xbb\xbf"):
            errors.append(f"{rel}:1: UTF-8 BOM is not allowed")

        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError as exc:
            errors.append(f"{rel}:1: file is not valid UTF-8: {exc}")
            continue

        for ch in BIDI_CHARS:
            idx = text.find(ch)
            if idx >= 0:
                errors.append(f"{rel}:{line_for_offset(text, idx)}: hidden/bidirectional Unicode character U+{ord(ch):04X}")

        if any(ord(ch) > 0x7F for ch in text):
            for idx, ch in enumerate(text):
                if ord(ch) > 0x7F:
                    errors.append(f"{rel}:{line_for_offset(text, idx)}: non-ASCII character U+{ord(ch):04X}")
                    break

        for match in HEADER_GUARD_RE.finditer(text):
            add_match(errors, rel, "reserved identifier used in header guard", text, match)

        for match in EMPTY_PARAMS_RE.finditer(text):
            add_match(errors, rel, "empty function parameter list; use (void)", text, match)

        for match in PUBLIC_TASK_FN_RE.finditer(text):
            add_match(errors, rel, "task entry function should have internal linkage", text, match)

        code_text = strip_c_comments(text)

        for label, pattern in ERROR_PATTERNS:
            for match in pattern.finditer(code_text):
                add_match(errors, rel, label, text, match)

        for label, pattern in WARNING_PATTERNS:
            for match in pattern.finditer(code_text):
                add_match(warnings, rel, label, text, match)

    if warnings:
        print("ECU MISRA-lite warnings:")
        for msg in warnings:
            print(f"  WARN: {msg}")

    if errors:
        print("ECU MISRA-lite errors:")
        for msg in errors:
            print(f"  ERROR: {msg}")
        return 1

    print("ECU MISRA-lite hygiene check passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
