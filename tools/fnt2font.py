import xml.etree.ElementTree as ET
import sys
from pathlib import Path

def convert_bmfont_xml(xml_path, output_path):
    tree = ET.parse(xml_path)
    root = tree.getroot()

    # --- Read font info ---
    info = root.find("info")
    common = root.find("common")
    page = root.find("pages/page")

    font_size = info.attrib.get("size", "0")
    tex_width = common.attrib.get("scaleW", "0")
    tex_height = common.attrib.get("scaleH", "0")
    texture_name = Path(page.attrib["file"]).stem

    lines = []
    lines.append(f"texture {texture_name} {tex_width} {tex_height}")
    lines.append(f"size {font_size}")

    # --- Glyphs ---
    chars = root.find("chars")
    for ch in chars.findall("char"):
        code = ch.attrib["id"]
        x = ch.attrib["x"]
        y = ch.attrib["y"]
        w = ch.attrib["width"]
        h = ch.attrib["height"]
        xoff = ch.attrib["xoffset"]
        yoff = ch.attrib["yoffset"]
        xadv = ch.attrib["xadvance"]

        lines.append(f"c {code} {x} {y} {w} {h} {xoff} {yoff} {xadv}")

    # --- Write output ---
    with open(output_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

    print(f"Written: {output_path}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python fnt2font.py input.fnt output.txt")
        sys.exit(1)

    convert_bmfont_xml(sys.argv[1], sys.argv[2])
