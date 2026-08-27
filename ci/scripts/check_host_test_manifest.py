#!/usr/bin/env python3
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
manifest_path = root / "ci" / "host_test_manifest.json"
manifest = json.loads(manifest_path.read_text())
actual = {
    p.relative_to(root).as_posix()
    for p in (root / "Core" / "Src").rglob("*.c")
}
listed = set(manifest)
missing = sorted(actual - listed)
extra = sorted(listed - actual)
errors = []
if missing:
    errors.append("production sources missing from host-test manifest:\n  " + "\n  ".join(missing))
if extra:
    errors.append("manifest entries that do not exist:\n  " + "\n  ".join(extra))
allowed = {"host-executed", "target-and-static", "target-build"}
for path, entry in sorted(manifest.items()):
    status = entry.get("status")
    target = entry.get("target", "")
    if status not in allowed:
        errors.append(f"{path}: invalid status {status!r}")
    if not target:
        errors.append(f"{path}: missing coverage target")
    if status != "host-executed" and not entry.get("rationale"):
        errors.append(f"{path}: non-host-executed source needs rationale")

if errors:
    print("ERROR host-test coverage manifest")
    for error in errors:
        print(error)
    sys.exit(1)

counts = {s: 0 for s in allowed}
for entry in manifest.values():
    counts[entry["status"]] += 1
print(
    "PASS host-test manifest covers all "
    f"{len(actual)} Core/Src files: "
    f"host-executed={counts['host-executed']} "
    f"target-and-static={counts['target-and-static']} "
    f"target-build={counts['target-build']}"
)
