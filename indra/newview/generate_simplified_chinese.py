#!/usr/bin/env python3
"""Generate Simplified Chinese XUI resources from Firestorm's Traditional Chinese translations.

Only visible text nodes and explicitly user-facing attributes are converted.
Control names, setting keys, resource IDs, paths, and other machine-readable
XML attributes are deliberately left unchanged.
"""
from __future__ import annotations

import sys
import xml.etree.ElementTree as ET
from pathlib import Path

from opencc import OpenCC


ROOT = Path(__file__).resolve().parent
SKINS = ROOT / "skins"
VISIBLE_ATTRIBUTES = {
    "label",
    "short_label",
    "tool_tip",
    "tooltip",
    "placeholder_text",
    "description",
    "help",
    "title",
}


def convert_element(element: ET.Element, converter: OpenCC) -> None:
    if element.text:
        element.text = converter.convert(element.text)
    if element.tail:
        element.tail = converter.convert(element.tail)

    for attribute in VISIBLE_ATTRIBUTES:
        value = element.attrib.get(attribute)
        if value:
            element.attrib[attribute] = converter.convert(value)

    for child in element:
        convert_element(child, converter)


def convert_xml(source: Path, target: Path, converter: OpenCC) -> None:
    parser = ET.XMLParser(target=ET.TreeBuilder(insert_comments=True))
    root = ET.fromstring(source.read_bytes(), parser=parser)
    convert_element(root, converter)
    target.parent.mkdir(parents=True, exist_ok=True)
    payload = ET.tostring(root, encoding="utf-8", xml_declaration=True)
    target.write_bytes(payload + b"\n")


def self_test(converter: OpenCC) -> None:
    sample = ET.fromstring(
        '<item label="繁體中文" name="Traditional Chinese" value="zh">'
        '<text>偏好設定</text></item>'
    )
    convert_element(sample, converter)
    if sample.attrib.get("label") != "简体中文":
        raise SystemExit("OpenCC self-test failed to convert visible label text.")
    if sample.attrib.get("name") != "Traditional Chinese":
        raise SystemExit("OpenCC self-test changed a control identifier.")
    if sample.attrib.get("value") != "zh":
        raise SystemExit("OpenCC self-test changed a language code.")
    if sample.findtext("text") != "偏好设置":
        raise SystemExit("OpenCC self-test failed to convert element text.")


def main() -> int:
    if not SKINS.is_dir():
        raise SystemExit(f"Skin directory not found: {SKINS}")

    converter = OpenCC("t2s")
    self_test(converter)
    source_dirs = sorted(
        path for path in SKINS.glob("**/xui/zh")
        if path.is_dir()
    )
    if not source_dirs:
        raise SystemExit("No Traditional Chinese xui/zh translation directories found.")

    converted_files = 0
    for source_dir in source_dirs:
        target_dir = source_dir.with_name("zh-Hans")
        for source_file in sorted(source_dir.rglob("*.xml")):
            relative = source_file.relative_to(source_dir)
            target_file = target_dir / relative
            try:
                convert_xml(source_file, target_file, converter)
            except ET.ParseError as exc:
                raise SystemExit(f"Invalid source XML {source_file}: {exc}") from exc
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
