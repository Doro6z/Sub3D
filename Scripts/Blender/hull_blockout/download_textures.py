"""
Sub3D — Texture Downloader (Polyhaven + ambientCG)
====================================================
Downloads PBR texture sets for the submarine.
Run from terminal (NOT Blender): python download_textures.py

All textures are CC0 (public domain).
"""

import os
import sys
import json
import urllib.request
import urllib.error

# ═══════════════════════════════════════════════════════════════
# CONFIG
# ═══════════════════════════════════════════════════════════════

OUTPUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "textures")
RESOLUTION = "2k"  # 1k, 2k, 4k
USER_AGENT = "Sub3D-TextureDownloader/1.0"

# Textures to download — (local_name, source, asset_id)
# V2: CLEANER textures — military sub, not rusty wreck
TEXTURE_LIST = [
    # POLYHAVEN — cleaner metal
    ("hull_clean",         "polyhaven", "green_metal_rust"),     # Green military hull (subtle wear)
    ("hull_grey",          "polyhaven", "painted_plaster"),      # Clean grey surface
    ("metal_panel",        "polyhaven", "metal_plate"),          # Clean metal plate for decks
    ("metal_brushed",      "polyhaven", "brushed_iron"),         # Brushed metal for details

    # AMBIENTCG — painted metal (cleaner options)
    ("painted_dark",       "ambientcg", "PaintedMetal012"),      # Dark painted metal, minimal rust
    ("painted_clean",      "ambientcg", "PaintedMetal009"),      # Clean painted metal
    ("painted_grey_v2",    "ambientcg", "PaintedMetal006"),      # Grey painted, light wear
    ("metal_diamond",      "ambientcg", "MetalPlates006"),       # Diamond plate floor
    ("metal_corrugated",   "ambientcg", "MetalPlates003"),       # Corrugated panels
    ("rubber_floor",       "ambientcg", "Rubber004"),            # Rubber floor (keep)
]

# Maps to download per texture
PBR_MAPS = ["diff", "nor_gl", "rough", "metal"]  # Polyhaven naming
AMBIENTCG_MAPS = ["Color", "NormalGL", "Roughness", "Metalness"]


# ═══════════════════════════════════════════════════════════════
# POLYHAVEN API
# ═══════════════════════════════════════════════════════════════

def fetch_json(url):
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        print(f"  HTTP Error {e.code}: {url}")
        return None
    except Exception as e:
        print(f"  Error: {e}")
        return None


def download_file(url, filepath):
    if os.path.exists(filepath):
        print(f"  SKIP (exists): {os.path.basename(filepath)}")
        return True
    print(f"  Downloading: {os.path.basename(filepath)}...")
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(req, timeout=120) as resp:
            with open(filepath, 'wb') as f:
                f.write(resp.read())
        size_mb = os.path.getsize(filepath) / (1024 * 1024)
        print(f"    OK ({size_mb:.1f} MB)")
        return True
    except Exception as e:
        print(f"    FAILED: {e}")
        return False


def download_polyhaven(local_name, asset_id):
    """Download PBR maps from Polyhaven API."""
    print(f"\n[Polyhaven] {asset_id} -> {local_name}")

    out_dir = os.path.join(OUTPUT_DIR, local_name)
    os.makedirs(out_dir, exist_ok=True)

    # Get file list from API
    files_url = f"https://api.polyhaven.com/files/{asset_id}"
    data = fetch_json(files_url)
    if not data:
        print(f"  Could not fetch asset info for '{asset_id}'")
        return False

    # Navigate to the texture files
    textures = data.get("Diffuse") or data.get("diffuse") or {}

    # Polyhaven structure: data -> {map_type} -> {resolution} -> {format} -> url
    map_names = {
        "diff": ["Diffuse", "diffuse", "Base Color"],
        "nor_gl": ["nor_gl", "Normal", "normal"],
        "rough": ["Rough", "rough", "Roughness"],
        "metal": ["Metal", "metal", "Metalness"],
        "arm": ["arm", "ARM"],
    }

    success = True
    for map_key, possible_names in map_names.items():
        found = False
        for name in possible_names:
            if name in data:
                map_data = data[name]
                # Try resolution
                res_data = map_data.get(RESOLUTION) or map_data.get("2k") or map_data.get("1k")
                if res_data:
                    # Try format
                    for fmt in ["png", "jpg", "exr"]:
                        if fmt in res_data:
                            url = res_data[fmt].get("url")
                            if url:
                                ext = fmt
                                filepath = os.path.join(out_dir, f"{map_key}.{ext}")
                                download_file(url, filepath)
                                found = True
                                break
                if found:
                    break
        if not found and map_key in ["diff", "nor_gl", "rough"]:
            print(f"  WARNING: {map_key} map not found for {asset_id}")
            success = False

    return success


# ═══════════════════════════════════════════════════════════════
# AMBIENTCG API
# ═══════════════════════════════════════════════════════════════

def download_ambientcg(local_name, asset_id):
    """Download PBR maps from ambientCG."""
    print(f"\n[ambientCG] {asset_id} -> {local_name}")

    out_dir = os.path.join(OUTPUT_DIR, local_name)
    os.makedirs(out_dir, exist_ok=True)

    # ambientCG direct download URL pattern
    # Format: https://ambientcg.com/get?file={assetId}_{resolution}-JPG.zip
    # Or individual files: predictable URL pattern

    # Try the API first
    api_url = f"https://ambientcg.com/api/v2/full_json?id={asset_id}&include=downloadData"
    data = fetch_json(api_url)

    if data and "foundAssets" in data:
        assets = data["foundAssets"]
        if assets:
            asset = assets[0] if isinstance(assets, list) else list(assets.values())[0]
            downloads = asset.get("downloadFolders", {})

            # Look for the right resolution
            for folder_name, folder_data in downloads.items():
                dl_list = folder_data.get("downloadFiletypeCategories", {})
                for cat_name, cat_data in dl_list.items():
                    for dl in cat_data.get("downloads", []):
                        if RESOLUTION.upper() in dl.get("attribute", "").upper():
                            url = dl.get("downloadLink") or dl.get("rawLink")
                            if url and url.endswith(".zip"):
                                # Download ZIP and extract
                                zip_path = os.path.join(out_dir, f"{asset_id}.zip")
                                if download_file(url, zip_path):
                                    import zipfile
                                    try:
                                        with zipfile.ZipFile(zip_path, 'r') as z:
                                            z.extractall(out_dir)
                                        os.remove(zip_path)
                                        # Rename files to standard names
                                        rename_ambientcg_files(out_dir, asset_id)
                                        return True
                                    except Exception as e:
                                        print(f"  Extract error: {e}")
                                        return False

    # Fallback: direct URL construction
    print(f"  Trying direct URL pattern...")
    base_url = f"https://ambientcg.com/get?file={asset_id}_{RESOLUTION.upper()}-JPG.zip"
    zip_path = os.path.join(out_dir, f"{asset_id}.zip")
    if download_file(base_url, zip_path):
        import zipfile
        try:
            with zipfile.ZipFile(zip_path, 'r') as z:
                z.extractall(out_dir)
            os.remove(zip_path)
            rename_ambientcg_files(out_dir, asset_id)
            return True
        except Exception as e:
            print(f"  Extract error: {e}")

    return False


def rename_ambientcg_files(directory, asset_id):
    """Rename ambientCG files to standard names (diff, nor_gl, rough, metal)."""
    renames = {
        "Color": "diff",
        "Colour": "diff",
        "NormalGL": "nor_gl",
        "Normal": "nor_gl",
        "Roughness": "rough",
        "Metalness": "metal",
        "Metallic": "metal",
        "AmbientOcclusion": "ao",
        "Displacement": "disp",
    }
    for fname in os.listdir(directory):
        for old_key, new_key in renames.items():
            if old_key in fname:
                ext = os.path.splitext(fname)[1]
                new_name = f"{new_key}{ext}"
                old_path = os.path.join(directory, fname)
                new_path = os.path.join(directory, new_name)
                if not os.path.exists(new_path):
                    os.rename(old_path, new_path)
                    print(f"  Renamed: {fname} -> {new_name}")
                break


# ═══════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════

def main():
    print("=" * 60)
    print("Sub3D — Texture Downloader")
    print(f"Output: {OUTPUT_DIR}")
    print(f"Resolution: {RESOLUTION}")
    print(f"Textures: {len(TEXTURE_LIST)}")
    print("=" * 60)

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    results = {"ok": [], "fail": []}

    for local_name, source, asset_id in TEXTURE_LIST:
        if source == "polyhaven":
            ok = download_polyhaven(local_name, asset_id)
        elif source == "ambientcg":
            ok = download_ambientcg(local_name, asset_id)
        else:
            print(f"  Unknown source: {source}")
            ok = False

        if ok:
            results["ok"].append(local_name)
        else:
            results["fail"].append(local_name)

    print("\n" + "=" * 60)
    print(f"DONE: {len(results['ok'])} OK, {len(results['fail'])} failed")
    if results["ok"]:
        print(f"  OK: {', '.join(results['ok'])}")
    if results["fail"]:
        print(f"  FAILED: {', '.join(results['fail'])}")
    print(f"\nTextures saved to: {OUTPUT_DIR}")
    print("Each folder contains: diff.png/jpg, nor_gl.png/jpg, rough.png/jpg, metal.png/jpg")
    print("\nNext: run assign_materials.py in Blender to setup PBR materials.")
    print("=" * 60)


if __name__ == "__main__":
    main()
