#!/usr/bin/env python3
"""Generate Simplified Chinese XUI resources from Firestorm's Traditional Chinese translations.

The upstream viewer keeps its Chinese XUI translations under xui/zh. OpenCC
converts the human-readable Chinese strings to Simplified Chinese while
preserving XML structure, control names, and resource identifiers.
"""
from __future__ import annotations

import sys
from pathlib import Path

from opencc import OpenCC


ROOT = Path(__file__).resolve().parent
SKINS = ROOT / "skins"


def main() -> int:
    if not SKINS.is_dir():
        raise SystemExit(f"Skin directory not found: {SKINS}")

    converter = OpenCC("t2s")
    source_dirs = sorted(
        path for path in SKINS.glob("**/xui/zh")
        if path.is_dir()
    )
    if not source_dirs:
        raise SystemExit("No Traditional Chinese xui/zh translation directories found.")

    converted_files = 0
    for source_dir in source_dirs:
        target_dir = source_dir.with_name("zh-Hans")
        target_dir.mkdir(parents=True, exist_ok=True)
        for source_file in sorted(source_dir.rglob("*.xml")):
            relative = source_file.relative_to(source_dir)
            target_file = target_dir / relative
            target_file.parent.mkdir(parents=True, exist_ok=True)
            original = source_file.read_text(encoding="utf-8-sig")
            converted = converter.convert(original)
            target_file.write_text(converted, encoding="utf-8", newline="\n")
            converted_files += 1

    if converted_files == 0:
        raise SystemExit("No XML translation files were generated.")

    print(
        f"Generated {converted_files} Simplified Chinese XUI files "
        f"from {len(source_dirs)} skin translation directories."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
