# Generates a single merged msdf font atlas from multiple font files


import os
import json
import re
import subprocess
import tempfile
from PIL import Image
from fontTools import ttLib
from fontTools.ttLib.tables._c_m_a_p import cmap_format_12

# --- Configuration ---

OUTPUT_DIR = "assets/bess_fonts"
OUTPUT_NAME = "bess_fonts_merged"
HEADER_DIR = "src/bess/ui/icons"

FONTS = [
    {
        "name": "Roboto",
        "input_path": "assets/fonts/Roboto/Roboto-Regular.ttf",
        "charset_range": (0x20, 0xFF),
        "offset": 0,
        "header": None,
    },
    {
        "name": "MaterialIcons",
        "input_path": "assets/fonts/icons/MaterialIcons-Regular.ttf",
        "input_header": os.path.join(HEADER_DIR, "MaterialIcons.h"),
        "output_header": os.path.join(HEADER_DIR, "MaterialIcons_Remapped.h"),
        "offset": 0,
        "namespace": "Bess::UI::Icons::MaterialIcons",
        "min_const": "ICON_MIN_MD",
        "max_const": "ICON_MAX_MD",
    },
    {
        "name": "FontAwesomeIcons",
        "input_path": "assets/fonts/icons/Font-Awesome-7-Free-Regular-400.otf",
        "input_header": os.path.join(HEADER_DIR, "FontAwesomeIcons.h"),
        "output_header": os.path.join(HEADER_DIR, "FontAwesomeIcons_Remapped.h"),
        "offset": 0x10000,
        "namespace": "Bess::UI::Icons::FontAwesomeIcons",
        "min_const": "SIZE_MIN_FA",
        "max_const": "SIZE_MAX_FA",
    },
    {
        "name": "CodIcons",
        "input_path": "assets/fonts/icons/codicon.ttf",
        "input_header": os.path.join(HEADER_DIR, "CodIcons.h"),
        "output_header": os.path.join(HEADER_DIR, "CodIcons_Remapped.h"),
        "offset": 0x30000,
        "namespace": "Bess::UI::Icons::CodIcons",
        "min_const": "ICON_MIN_CI",
        "max_const": "ICON_MAX_CI",
    },
    {
        "name": "ComponentIcons",
        "input_path": "assets/fonts/icons/ComponentIcons.ttf",
        "input_header": os.path.join(HEADER_DIR, "ComponentIcons.h"),
        "output_header": os.path.join(HEADER_DIR, "ComponentIcons_Remapped.h"),
        "offset": 0x40000,
        "namespace": "Bess::UI::Icons::ComponentIcons",
        "min_const": "SIZE_MIN_CI",
        "max_const": "SIZE_MAX_CI",
    },
]


def parse_original_header(file_path):
    glyphs = []
    if not file_path or not os.path.exists(file_path):
        return []
    with open(file_path, "r") as f:
        content = f.read()

    matches = re.findall(
        r'constexpr\s+(?:auto|char)\s+(\w+)(?:\[\])?\s*=\s*".*";\s*//\s*U\+([0-9a-fA-F]+)',
        content,
    )
    if matches:
        return [(m[0], int(m[1], 16)) for m in matches]

    matches = re.findall(
        r'constexpr\s+(?:auto|char)\s+(\w+)(?:\[\])?\s*=\s*"((?:\\[xX][0-9a-fA-F]{2})+)"',
        content,
    )
    for name, escapes in matches:
        try:
            byte_data = bytes.fromhex(escapes.replace("\\x", "").replace("\\X", ""))
            glyphs.append((name, ord(byte_data.decode("utf-8"))))
        except Exception:
            continue
    return glyphs


def codepoint_to_utf8_escape(cp):
    try:
        return "".join(f"\\x{b:02x}" for b in chr(cp).encode("utf-8"))
    except Exception:
        return ""


def remap_font_file(input_path, output_path, offset):
    font = ttLib.TTFont(input_path)
    old_cmap = font.getBestCmap()
    new_cmap_table = cmap_format_12(12)
    new_cmap_table.platformID, new_cmap_table.platEncID, new_cmap_table.language = (
        3,
        10,
        0,
    )
    new_cmap_table.cmap = {cp + offset: name for cp, name in old_cmap.items()}
    font["cmap"].tables = [new_cmap_table]
    font.save(output_path)


def main():
    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)
    temp_dir = tempfile.mkdtemp()
    print(f"Using temp directory: {temp_dir}")

    sub_atlases = []

    for config in FONTS:
        print(f"Processing {config['name']}...")
        remapped_ttf_path = os.path.join(OUTPUT_DIR, f"{config['name']}_remapped.ttf")
        if config["offset"] != 0:
            remap_font_file(config["input_path"], remapped_ttf_path, config["offset"])
        elif config["name"] != "Roboto":
            import shutil

            shutil.copy(config["input_path"], remapped_ttf_path)

        json_out, png_out = os.path.join(
            temp_dir, f"{config['name']}.json"
        ), os.path.join(temp_dir, f"{config['name']}.png")
        cmd = [
            "msdf-atlas-gen",
            "-type",
            "mtsdf",
            "-size",
            "32",
            "-pxrange",
            "4",
            "-outerpxpadding",
            "8",
            "-imageout",
            png_out,
            "-json",
            json_out,
            "-font",
            config["input_path"],
        ]

        header_glyphs = parse_original_header(config.get("input_header"))
        if header_glyphs:
            charset_path = os.path.join(temp_dir, f"{config['name']}_charset.txt")
            with open(charset_path, "w") as f:
                for _, cp in header_glyphs:
                    f.write(f"{cp}\n")
            cmd.extend(["-charset", charset_path])
        elif config["name"] == "Roboto":
            charset_path = os.path.join(temp_dir, f"{config['name']}_charset.txt")
            with open(charset_path, "w") as f:
                for cp in range(
                    config["charset_range"][0], config["charset_range"][1] + 1
                ):
                    f.write(f"{cp}\n")
            cmd.extend(["-charset", charset_path])
        else:
            cmd.append("-allglyphs")

        subprocess.run(cmd, capture_output=True, check=True)
        with open(json_out, "r") as f:
            data = json.load(f)
        sub_atlases.append({"data": data, "img": Image.open(png_out), "config": config})

        if "output_header" in config and config["output_header"]:
            all_cps = [g[1] + config["offset"] for g in header_glyphs]
            if all_cps:
                with open(config["output_header"], "w") as f:
                    f.write("#pragma once\n\n")
                    f.write(f"namespace {config['namespace']} {{\n")
                    f.write(
                        f"    constexpr auto {config['min_const']} = 0x{min(all_cps):x};\n"
                    )
                    f.write(
                        f"    constexpr auto {config['max_const']} = 0x{max(all_cps):x};\n\n"
                    )
                    for name, original_cp in header_glyphs:
                        new_cp = original_cp + config["offset"]
                        f.write(
                            f'    constexpr char {name}[] = "{codepoint_to_utf8_escape(new_cp)}"; // U+{new_cp:x}\n'
                        )
                    f.write(f"}} // namespace {config['namespace']}\n")

    print("Stitching...")
    total_width = max(a["img"].width for a in sub_atlases)
    total_height = sum(a["img"].height for a in sub_atlases)
    merged_img = Image.new("RGBA", (total_width, total_height))
    merged_glyphs = []

    current_y_offset_from_bottom = 0
    for atlas in sub_atlases:
        paste_y = total_height - current_y_offset_from_bottom - atlas["img"].height
        merged_img.paste(atlas["img"], (0, paste_y))
        offset = atlas["config"]["offset"]
        for g in atlas["data"]["glyphs"]:
            if "unicode" in g:
                g["unicode"] += offset
                if "atlasBounds" in g:
                    g["atlasBounds"]["bottom"] += current_y_offset_from_bottom
                    g["atlasBounds"]["top"] += current_y_offset_from_bottom
                merged_glyphs.append(g)
        current_y_offset_from_bottom += atlas["img"].height

    merged_img.save(os.path.join(OUTPUT_DIR, f"{OUTPUT_NAME}.png"))
    final_json = {
        "atlas": {
            "type": "mtsdf",
            "distanceRange": 4,
            "size": 32,
            "width": total_width,
            "height": total_height,
            "yOrigin": "bottom",
        },
        "metrics": sub_atlases[0]["data"]["metrics"],
        "glyphs": merged_glyphs,
    }
    with open(os.path.join(OUTPUT_DIR, f"{OUTPUT_NAME}.json"), "w") as f:
        json.dump(final_json, f)
    print(
        f"Successfully generated merged atlas ({len(merged_glyphs)} glyphs, size {total_width}x{total_height})"
    )


if __name__ == "__main__":
    main()
