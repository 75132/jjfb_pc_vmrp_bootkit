#!/usr/bin/env python3
import csv
from pathlib import Path

rows = []
with open("reports/e10a31d_method0_instruction_trace.csv", encoding="utf-8") as f:
    for row in csv.DictReader(f):
        s = int(row["seq"])
        if 4995 <= s <= 5056:
            rows.append(row)
print("n", len(rows))
for r in rows:
    print(
        f"{r['seq']:>5} {r['pc']:>10} lr={r['lr']:>10} "
        f"r0={r['r0']:>10} r1={r['r1']:>10} r2={r['r2']:>10} r3={r['r3']:>10} "
        f"r9={r['r9']:>10} {r['kind']}/{r['target']}"
    )
