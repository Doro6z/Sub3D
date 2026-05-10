"""
Sub3D — Meshy image-to-3d batch launcher for crew characters.

Lance les 4 personnages (Crew, Captain, Engineer, Concierge) en parallèle
depuis les images _clean.png dans references/ (générées via ChatGPT).

Usage:
    $env:MESHY_API_KEY = "msy-..."
    python launch_meshy.py
"""

import base64
import json
import os
import sys
import time
import threading
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import HTTPError

API_KEY = os.environ.get("MESHY_API_KEY")
if not API_KEY:
    print("ERROR: MESHY_API_KEY not set in environment.")
    sys.exit(1)

BASE_URL = "https://api.meshy.ai/openapi/v1/image-to-3d"
HEADERS = {
    "Authorization": f"Bearer {API_KEY}",
    "Content-Type": "application/json",
}

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parents[2]
OUTPUT_DIR = PROJECT_ROOT / "Content" / "Sub3D" / "Assets" / "Characters_v2"
REFS_DIR = SCRIPT_DIR / "references"
STATE_PATH = SCRIPT_DIR / "meshy_state.json"

POLL_INTERVAL_S = 10
POLL_TIMEOUT_S = 30 * 60

CHARACTERS = {
    "Crew":      {"image_filename": "crew_clean.png"},
    "Captain":   {"image_filename": "captain_clean.png"},
    "Engineer":  {"image_filename": "engineer_clean.png"},
    "Concierge": {"image_filename": "concierge_clean.png"},
}

COMMON_PAYLOAD = {
    "ai_model": "meshy-6",
    "topology": "quad",
    "target_polycount": 30000,
    "symmetry_mode": "on",
    "should_remesh": True,
    "should_texture": True,
    "enable_pbr": True,
    "target_formats": ["fbx", "glb"],
}


def http_request(method, url, body=None):
    data = json.dumps(body).encode("utf-8") if body is not None else None
    req = Request(url, method=method, headers=HEADERS, data=data)
    try:
        with urlopen(req, timeout=120) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except HTTPError as e:
        body_text = e.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"HTTP {e.code} on {method} {url}: {body_text}") from e


def http_download(url, dest):
    req = Request(url)
    with urlopen(req, timeout=180) as resp:
        dest.write_bytes(resp.read())


def image_to_data_uri(path: Path) -> str:
    suffix = path.suffix.lower().lstrip(".")
    mime = "image/png" if suffix == "png" else f"image/{suffix}"
    b64 = base64.b64encode(path.read_bytes()).decode("ascii")
    return f"data:{mime};base64,{b64}"


_state_lock = threading.Lock()


def load_state():
    if STATE_PATH.exists():
        return json.loads(STATE_PATH.read_text(encoding="utf-8"))
    return {name: {} for name in CHARACTERS}


def save_state(state):
    with _state_lock:
        STATE_PATH.write_text(json.dumps(state, indent=2), encoding="utf-8")


def update_state(state, name, **kwargs):
    state.setdefault(name, {}).update(kwargs)
    save_state(state)


def create_image_to_3d(image_path: Path):
    payload = dict(COMMON_PAYLOAD)
    payload["image_url"] = image_to_data_uri(image_path)
    resp = http_request("POST", BASE_URL, payload)
    return resp["result"]


def poll_task(name, task_id):
    deadline = time.time() + POLL_TIMEOUT_S
    last_progress = -1
    while time.time() < deadline:
        data = http_request("GET", f"{BASE_URL}/{task_id}")
        status = data.get("status")
        progress = data.get("progress", 0)
        if progress != last_progress:
            print(f"[{name}] {status} {progress}%", flush=True)
            last_progress = progress
        if status == "SUCCEEDED":
            return data
        if status == "FAILED":
            err = data.get("task_error", {}).get("message", "unknown")
            raise RuntimeError(f"FAILED: {err}")
        time.sleep(POLL_INTERVAL_S)
    raise TimeoutError(f"timeout after {POLL_TIMEOUT_S}s")


def download_assets(name, task_data, char_dir):
    char_dir.mkdir(parents=True, exist_ok=True)
    fbx_url = task_data.get("model_urls", {}).get("fbx")
    glb_url = task_data.get("model_urls", {}).get("glb")
    if fbx_url:
        http_download(fbx_url, char_dir / f"SK_{name}_Mesh.fbx")
        print(f"[{name}] downloaded FBX", flush=True)
    if glb_url:
        http_download(glb_url, char_dir / f"SK_{name}_Mesh.glb")
        print(f"[{name}] downloaded GLB", flush=True)
    textures = (task_data.get("texture_urls") or [{}])[0]
    for tex_type, url in textures.items():
        if url:
            http_download(url, char_dir / f"T_{name}_{tex_type}.png")
            print(f"[{name}] downloaded T_{name}_{tex_type}.png", flush=True)
    thumb = task_data.get("thumbnail_url")
    if thumb:
        http_download(thumb, char_dir / "thumbnail.png")


def run_character(name, config, state):
    char_dir = OUTPUT_DIR / name
    image_path = REFS_DIR / config["image_filename"]
    s = state.setdefault(name, {})

    if not image_path.exists():
        print(f"[{name}] ERROR: ref image not found: {image_path}", flush=True)
        update_state(state, name, error=f"ref missing: {image_path}")
        return

    try:
        if not s.get("task_id"):
            print(f"[{name}] uploading image + creating task ({image_path.stat().st_size//1024} KB)", flush=True)
            s["task_id"] = create_image_to_3d(image_path)
            update_state(state, name, **s)
        else:
            print(f"[{name}] resuming task {s['task_id']}", flush=True)

        if s.get("status") != "SUCCEEDED":
            task_data = poll_task(name, s["task_id"])
            update_state(state, name, status="SUCCEEDED")
        else:
            task_data = http_request("GET", f"{BASE_URL}/{s['task_id']}")

        if not s.get("downloaded"):
            download_assets(name, task_data, char_dir)
            update_state(state, name, downloaded=True)

        print(f"[{name}] DONE -> {char_dir}", flush=True)
    except Exception as e:
        print(f"[{name}] ERROR: {e}", flush=True)
        update_state(state, name, error=str(e))


def main():
    missing = [n for n, c in CHARACTERS.items() if not (REFS_DIR / c["image_filename"]).exists()]
    if missing:
        print(f"ERROR: missing clean ref images for: {', '.join(missing)}")
        print(f"Expected in: {REFS_DIR}")
        for n in missing:
            print(f"  - {CHARACTERS[n]['image_filename']}")
        sys.exit(1)

    state = load_state()
    threads = []
    for name, config in CHARACTERS.items():
        t = threading.Thread(target=run_character, args=(name, config, state), name=name)
        t.start()
        threads.append(t)
    for t in threads:
        t.join()

    print("\n=== SUMMARY ===")
    for name in CHARACTERS:
        s = state.get(name, {})
        if s.get("downloaded"):
            print(f"  OK  {name}")
        elif s.get("error"):
            print(f"  ERR {name}: {s['error']}")
        else:
            print(f"  ??? {name}: {s}")


if __name__ == "__main__":
    main()
