import json
import os
from pathlib import Path

SCHEMA_FILE = Path("c:/Dev/Sub3D/reports/data/sub3d_data_schema.json")
OUT_DIR = Path("c:/Dev/Sub3D/reports/data/schema")

def split():
    with open(SCHEMA_FILE, 'r', encoding='utf-8') as f:
        data = json.load(f)

    meta = data.get('meta', {})
    domains = data.get('domains', [])
    tables = data.get('tables', [])

    by_domain = {}
    for t in tables:
        did = t['domain']
        if did not in by_domain:
            by_domain[did] = []
        by_domain[did].append(t)

    # Clean file naming mapping
    names = {
        1: "01_hull",
        2: "02_modules",
        3: "03_catalog",
        4: "04_enemies",
        5: "05_items",
        6: "06_world",
        7: "07_breach",
        8: "08_mission",
        9: "09_difficulty"
    }

    for did, domain_tables in by_domain.items():
        fname = names.get(did, f"{did:02d}_unknown") + ".json"
        domain_info = next((d for d in domains if d['id'] == did), {"id": did, "name": "Unknown"})
        
        output = {
            "meta": {
                "domain": domain_info['name'],
                "parentId": f"domain_{did}",
                "description": f"Schema definition for {domain_info['name']}."
            },
            "tables": domain_tables
        }
        
        out_path = OUT_DIR / fname
        with open(out_path, 'w', encoding='utf-8') as f:
            json.dump(output, f, indent=2, ensure_ascii=False)
        print(f"Created: {out_path}")

    # Also save metadata for domains separately if needed for the pipeline
    domain_meta = {
        "domains": domains
    }
    with open(OUT_DIR / "00_domains.json", 'w', encoding='utf-8') as f:
        json.dump(domain_meta, f, indent=2, ensure_ascii=False)
    print(f"Created: 00_domains.json")

if __name__ == "__main__":
    split()
