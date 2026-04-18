#!/usr/bin/env python3
"""
Sub3D Data Pipeline Tool
========================
Reads sub3d_data_schema.json and produces:
  1. sub3d_data_matrix.html  — interactive schema visualizer (schema embedded inline)
  2. ue_csv/<TableName>.csv  — UE DataTable-compatible CSV templates (Name column first)

Usage:
  python sub3d_data_pipeline.py              # generate all outputs
  python sub3d_data_pipeline.py --html-only  # only regenerate HTML
  python sub3d_data_pipeline.py --csv-only   # only generate CSV templates
  python sub3d_data_pipeline.py --tables Enemy_Stats Items_Stats  # specific tables

No third-party dependencies required.
"""

import json
import csv
import os
import sys
import argparse
import re
from pathlib import Path

SCRIPT_DIR   = Path(__file__).parent
SCHEMA_FILE  = SCRIPT_DIR / "sub3d_data_schema.json"
TEMPLATE_FILE= SCRIPT_DIR / "_template_matrix.html"
HTML_OUT     = SCRIPT_DIR / "sub3d_data_matrix.html"
CSV_DIR      = SCRIPT_DIR / "ue_csv"


# ─────────────────────────────────────────────
# LOAD SCHEMA
# ─────────────────────────────────────────────
def load_schema():
    schema_dir = SCRIPT_DIR / "schema"
    if not schema_dir.exists():
        sys.exit(f"[ERROR] Schema directory not found: {schema_dir}")
    
    # Load domain meta first
    domain_meta_file = schema_dir / "00_domains.json"
    if not domain_meta_file.exists():
        sys.exit(f"[ERROR] Domain metadata not found: {domain_meta_file}")
    
    with open(domain_meta_file, 'r', encoding='utf-8') as f:
        full_schema = json.load(f)
    
    if "tables" not in full_schema:
        full_schema["tables"] = []
    
    # Load all other JSON files
    for json_file in sorted(schema_dir.glob("*.json")):
        if json_file.name == "00_domains.json":
            continue
            
        with open(json_file, 'r', encoding='utf-8') as f:
            domain_data = json.load(f)
            tables = domain_data.get("tables", [])
            # Inject parent info if needed for cluster visualization
            parent_id = domain_data.get("meta", {}).get("parentId")
            for t in tables:
                if parent_id:
                    t["parentId"] = parent_id
            full_schema["tables"].extend(tables)
            
    return full_schema


# ─────────────────────────────────────────────
# VALIDATE SCHEMA (FK integrity check)
# ─────────────────────────────────────────────
def validate_schema(schema):
    table_ids = {t['id'] for t in schema['tables']}
    errors = []

    for table in schema['tables']:
        for rel in table.get('relations', []):
            if rel['target'] not in table_ids:
                errors.append(
                    f"  Table '{table['id']}': relation target '{rel['target']}' not found in schema."
                )
        for col in table.get('columns', []):
            if col.get('fk'):
                ref_table = col['fk'].split('.')[0]
                if ref_table not in table_ids:
                    errors.append(
                        f"  Table '{table['id']}', column '{col['name']}': FK ref table '{ref_table}' not found."
                    )

    if errors:
        print(f"[WARN] Schema validation found {len(errors)} issue(s):")
        for e in errors:
            print(e)
    else:
        print(f"[OK]   Schema validated — {len(table_ids)} tables, all FK targets resolved.")
    return len(errors) == 0


# ─────────────────────────────────────────────
# GENERATE HTML
# ─────────────────────────────────────────────
def generate_html(schema):
    if not TEMPLATE_FILE.exists():
        sys.exit(f"[ERROR] HTML template not found: {TEMPLATE_FILE}")

    schema_json = json.dumps(schema, indent=2, ensure_ascii=False)

    with open(TEMPLATE_FILE, 'r', encoding='utf-8') as f:
        html = f.read()

    # Replace placeholder with actual schema data
    html = html.replace('SCHEMA_PLACEHOLDER', schema_json)

    with open(HTML_OUT, 'w', encoding='utf-8') as f:
        f.write(html)

    table_count = len(schema['tables'])
    rel_count   = sum(len(t.get('relations', [])) for t in schema['tables'])
    print(f"[OK]   HTML generated: {HTML_OUT}")
    print(f"       {table_count} tables · {rel_count} declared relations")


# ─────────────────────────────────────────────
# GENERATE CSV TEMPLATES
# ─────────────────────────────────────────────
def ue_type(col):
    """Map schema type to a UE-friendly comment hint."""
    t = col.get('type', 'string')
    if t == 'bool':    return 'bool'
    if t == 'int':     return 'int32'
    if t == 'float':   return 'float'
    if t == 'enum':    return 'string (enum)'
    return 'string'

def generate_csv(schema, target_tables=None):
    CSV_DIR.mkdir(exist_ok=True)
    tables = schema['tables']

    if target_tables:
        tables = [t for t in tables if t['id'] in target_tables]
        not_found = set(target_tables) - {t['id'] for t in tables}
        if not_found:
            print(f"[WARN] Tables not found in schema: {', '.join(not_found)}")

    generated = 0
    for table in tables:
        cols = table.get('columns', [])
        # UE DataTable: first column must be "Name" (row name)
        headers = ['Name'] + [c['name'] for c in cols if not c.get('pk')]
        # Type hint row (UE ignores it but useful for editors)
        type_row = ['---'] + [ue_type(col) for col in cols if not col.get('pk')]
        
        # Output file write
        out_file = CSV_DIR / f"{table['id']}.csv"
        with open(out_file, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f, delimiter=',')
            writer.writerow(headers)
            writer.writerow(type_row)
            
            # Write actual rows if they exist
            rows = table.get('rows', [])
            if not rows:
                # Still provide an example empty row if no data exists
                row_data = ['ROW_EXAMPLE']
                for col in cols:
                    if col.get('pk'): continue
                    row_data.append(str(col.get('default', '')))
                writer.writerow(row_data)
            else:
                # Write data rows
                for row in rows:
                    csv_row = []
                    # 1. Primary Key (UE 'Name' column)
                    pk_col = next((c for c in cols if c.get('pk')), None)
                    if pk_col:
                        csv_row.append(row.get(pk_col['name'], 'UnnamedRow'))
                    else:
                        csv_row.append('UnnamedRow')
                    
                    # 2. Other columns
                    for col in cols:
                        if col.get('pk'): continue
                        csv_row.append(row.get(col['name'], ''))
                    writer.writerow(csv_row)

        generated += 1

    print(f"[OK]   CSV templates: {generated} file(s) written to {CSV_DIR}/")


# ─────────────────────────────────────────────
# STATS REPORT
# ─────────────────────────────────────────────
def print_report(schema):
    tables = schema['tables']
    by_domain = {}
    for t in tables:
        d = t['domain']
        by_domain.setdefault(d, []).append(t)

    domain_map = {d['id']: d for d in schema['domains']}

    print("\n── Schema Report ─────────────────────────────")
    total_rel = 0
    for did, d in sorted(domain_map.items()):
        domain_tables = by_domain.get(did, [])
        high  = sum(1 for t in domain_tables if t['priority'] == 'high')
        med   = sum(1 for t in domain_tables if t['priority'] == 'medium')
        low   = sum(1 for t in domain_tables if t['priority'] == 'low')
        rels  = sum(len(t.get('relations', [])) for t in domain_tables)
        total_rel += rels
        print(f"  D{did:02d} {d['name']:<28} {len(domain_tables):2d} tables  "
              f"(H:{high} M:{med} L:{low})  {rels:2d} relations")

    print(f"  {'─'*60}")
    print(f"  {'TOTAL':<32} {len(tables):2d} tables  {total_rel:2d} relations")
    print()


# ─────────────────────────────────────────────
# MAIN
# ─────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description='Sub3D Data Pipeline Tool')
    parser.add_argument('--html-only',  action='store_true', help='Only generate HTML')
    parser.add_argument('--csv-only',   action='store_true', help='Only generate CSV templates')
    parser.add_argument('--tables', nargs='+', metavar='TABLE', help='Filter specific tables for CSV')
    parser.add_argument('--validate',   action='store_true', help='Validate schema only, no output')
    parser.add_argument('--report',     action='store_true', help='Print schema stats report')
    args = parser.parse_args()

    print(f"\nSub3D Data Pipeline")
    print(f"Schema:   {SCHEMA_FILE}")

    schema = load_schema()
    is_valid = validate_schema(schema)

    if args.validate:
        sys.exit(0 if is_valid else 1)

    if args.report:
        print_report(schema)
        sys.exit(0)

    do_html = not args.csv_only
    do_csv  = not args.html_only

    if do_html:
        generate_html(schema)

    if do_csv:
        generate_csv(schema, target_tables=args.tables)

    if not args.html_only and not args.csv_only:
        print_report(schema)

    print("[DONE]")


if __name__ == '__main__':
    main()
