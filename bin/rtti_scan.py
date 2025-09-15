#!/usr/bin/env python3
import argparse
import os
import re
import sys
import csv
from collections import deque, defaultdict
from typing import Dict, List, Optional, Tuple, Set

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])\s*([^">]+)\s*[">]')
# Matches lines like:
#   static constexpr auto RTTI{ RTTI::TESRace };
#   inline static constexpr auto RTTI { RTTI::Actor };
RTTI_RE = re.compile(
    r'\b(?:inline\s+)?(?:static\s+)?constexpr\s+auto\s+RTTI\s*\{\s*RTTI::([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*\}\s*;',
    re.ASCII
)

# IDs_RTTI.h entries:
#   inline constexpr REL::ID AIProcess__PendingActorHeadData{ 2773423 };
#   inline constexpr REL::ID TESRace{ 1234567 };
ID_ENTRY_RE = re.compile(
    r'\binline\s+constexpr\s+REL::ID\s+([A-Za-z_]\w*)\s*\{\s*(\d+)\s*\}\s*;',
    re.ASCII
)

HDR_EXTS = {'.h', '.hh', '.hpp', '.hxx', '.inl', '.ipp'}

def is_header(path: str) -> bool:
    return os.path.splitext(path)[1].lower() in HDR_EXTS

def norm(path: str) -> str:
    return os.path.normpath(os.path.realpath(path))

def resolve_include(token: str, base_dir: str, include_dirs: List[str], angled: bool, cache: dict) -> Optional[str]:
    key = (token, base_dir, tuple(include_dirs), angled)
    if key in cache:
        return cache[key]

    candidates = []
    if not angled:
        candidates.append(norm(os.path.join(base_dir, token)))
    for inc in include_dirs:
        candidates.append(norm(os.path.join(inc, token)))

    for cand in candidates:
        if os.path.isfile(cand):
            cache[key] = cand
            return cand

    # Slow fallback: search by basename
    base = os.path.basename(token)
    for inc in include_dirs:
        for root, _, files in os.walk(inc):
            if base in files:
                cand = norm(os.path.join(root, base))
                # Prefer exact suffix match (…/token)
                if cand.endswith(os.sep + token):
                    cache[key] = cand
                    return cand
                cache[key] = cand
                return cand

    cache[key] = None
    return None

def scan_file(path: str):
    """Yield ('include', angled(bool), token, lineno) and ('rtti', name, lineno)."""
    try:
        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            for i, line in enumerate(f, 1):
                m = INCLUDE_RE.match(line)
                if m:
                    angled = (m.group(1) == '<')
                    token = m.group(2).strip()
                    yield ('include', angled, token, i)
                for rm in RTTI_RE.finditer(line):
                    name = rm.group(1)  # e.g., 'TESRace' or 'AIProcess::PendingActorHeadData'
                    yield ('rtti', name, i)
    except (FileNotFoundError, PermissionError) as e:
        print(f"[warn] Could not read {path}: {e}", file=sys.stderr)

def walk_includes(start_file: str, include_dirs: List[str], follow_angle: bool, verbose: bool):
    """BFS through include graph from start_file."""
    q = deque([norm(start_file)])
    visited: Set[str] = set()
    include_cache: dict = {}
    rtti_hits: List[Tuple[str, str, int]] = []   # (rtti_name, file, line)
    missing = defaultdict(int)

    while q:
        cur = q.popleft()
        if cur in visited:
            continue
        visited.add(cur)

        if verbose:
            print(f"[scan] {cur}", file=sys.stderr)

        base_dir = os.path.dirname(cur)
        for kind, *rest in scan_file(cur):
            if kind == 'include':
                angled, token, lineno = rest
                if not follow_angle and angled:
                    continue
                target = resolve_include(token, base_dir, include_dirs, angled, include_cache)
                if target and is_header(target):
                    if target not in visited:
                        q.append(target)
                else:
                    missing[token] += 1
            else:  # 'rtti'
                name, lineno = rest
                rtti_hits.append((name, cur, lineno))

    return rtti_hits, visited, missing

def find_ids_rtti_files(include_dirs: List[str], preferred: Optional[str]) -> List[str]:
    if preferred:
        p = norm(preferred)
        if os.path.isfile(p):
            return [p]
        print(f"[warn] --ids-file given but not found: {p}", file=sys.stderr)

    found = []
    for inc in include_dirs:
        for root, _, files in os.walk(inc):
            for fn in files:
                if fn.lower() == 'ids_rtti.h':
                    found.append(norm(os.path.join(root, fn)))
    # Dedup while preserving order
    seen = set()
    out = []
    for f in found:
        if f not in seen:
            seen.add(f)
            out.append(f)
    return out

def parse_ids_rtti(paths: List[str], verbose: bool) -> Dict[str, Tuple[str, int]]:
    """
    Return map: symbol_name -> (symbol_name, numeric_id)
    """
    mapping: Dict[str, Tuple[str, int]] = {}
    for p in paths:
        try:
            if verbose:
                print(f"[parse] IDs_RTTI: {p}", file=sys.stderr)
            with open(p, 'r', encoding='utf-8', errors='ignore') as f:
                for line in f:
                    m = ID_ENTRY_RE.search(line)
                    if m:
                        sym = m.group(1)           # e.g., AIProcess__PendingActorHeadData or TESRace
                        val = int(m.group(2))      # numeric ID
                        mapping[sym] = (sym, val)
        except Exception as e:
            print(f"[warn] Failed parsing {p}: {e}", file=sys.stderr)
    return mapping

def rtti_to_symbol_candidates(rtti_name: str) -> List[str]:
    """
    Map 'AIProcess::PendingActorHeadData' -> ['AIProcess__PendingActorHeadData', 'RTTI_AIProcess__PendingActorHeadData']
    Map 'TESRace' -> ['TESRace', 'RTTI_TESRace']
    """
    base = rtti_name.replace('::', '__')
    cands = [base, f"RTTI_{base}"]

    # Some projects also use a trailing '_RTTI' or prefix variations—add a few generous fallbacks:
    cands += [f"{base}_RTTI", f"{base}RTTI", f"RTTI__{base}"]
    return cands

def main():
    ap = argparse.ArgumentParser(
        description="Find RTTI IDs by recursively following includes from a starting header and cross-referencing IDs_RTTI.h."
    )
    ap.add_argument("start", help="Path to your lean header file (start of traversal).")
    ap.add_argument(
        "-I", "--include-dir", action="append", default=[],
        help="Add an include directory to resolve #includes (can be used multiple times)."
    )
    ap.add_argument(
        "--follow-angle", action="store_true",
        help="Also follow #include <...> headers if resolvable in provided -I dirs."
    )
    ap.add_argument(
        "--ids-file", default=None,
        help="Explicit path to IDs_RTTI.h. If omitted, will search -I dirs for IDs_RTTI.h."
    )
    ap.add_argument(
        "-o", "--out-csv", default="rtti_report.csv",
        help="CSV path for detailed results (default: rtti_report.csv)."
    )
    ap.add_argument(
        "-u", "--out-unique", default="rtti_unique.txt",
        help="Text file for unique RTTI + ID mappings (default: rtti_unique.txt)."
    )
    ap.add_argument("-v", "--verbose", action="store_true", help="Verbose output.")
    args = ap.parse_args()

    start = norm(args.start)
    if not os.path.isfile(start):
        print(f"[error] Start file not found: {start}", file=sys.stderr)
        sys.exit(1)

    include_dirs = [norm(d) for d in args.include_dir if os.path.isdir(d)]
    if args.verbose:
        print(f"[info] Start: {start}", file=sys.stderr)
        for d in include_dirs:
            print(f"[info] Include dir: {d}", file=sys.stderr)
        print(f"[info] Follow angle includes: {args.follow_angle}", file=sys.stderr)

    # 1) Walk include graph and collect RTTI hits
    rtti_hits, visited, missing = walk_includes(
        start, include_dirs, args.follow_angle, args.verbose
    )

    # 2) Parse IDs_RTTI.h to build symbol -> id map
    ids_files = find_ids_rtti_files(include_dirs, args.ids_file)
    if not ids_files:
        print("[warn] No IDs_RTTI.h found. Provide --ids-file or ensure it's under -I paths.", file=sys.stderr)
    id_map = parse_ids_rtti(ids_files, args.verbose)

    # 3) Cross-reference RTTI names to symbols to numeric IDs
    resolved_rows = []
    unresolved: Dict[str, int] = defaultdict(int)  # count of unresolved RTTI names

    for name, file, line in rtti_hits:
        sym = None
        val = None
        for cand in rtti_to_symbol_candidates(name):
            if cand in id_map:
                sym, val = id_map[cand]
                break
        if sym is None:
            unresolved[name] += 1
        resolved_rows.append((name, sym, val, file, line))

    # 4) Output CSV
    os.makedirs(os.path.dirname(os.path.abspath(args.out_csv)) or ".", exist_ok=True)
    with open(args.out_csv, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["RTTI", "ID_Symbol", "ID_Value", "File", "Line"])
        for name, sym, val, file, line in sorted(resolved_rows, key=lambda x: (x[0], x[3], x[4])):
            w.writerow([f"RTTI::{name}", sym if sym else "", val if val is not None else "", file, line])

    # 5) Output unique list (deduped) with IDs if available
    uniques = {}
    for name, sym, val, _, _ in resolved_rows:
        key = f"RTTI::{name}"
        # Prefer first resolved value
        if key not in uniques:
            uniques[key] = (sym, val)
        else:
            # If previously unresolved, but this one resolved, upgrade
            if uniques[key][1] is None and val is not None:
                uniques[key] = (sym, val)

    with open(args.out_unique, "w", encoding="utf-8") as f:
        for k in sorted(uniques.keys()):
            sym, val = uniques[k]
            if sym and val is not None:
                f.write(f"{k} => {sym} ({val})\n")
            else:
                f.write(f"{k} => (unresolved)\n")

    print(f"Wrote {len(resolved_rows)} hits to {args.out_csv}")
    print(f"Wrote {len(uniques)} unique RTTIs to {args.out_unique}")
    if missing:
        miss_list = ", ".join(sorted(missing.keys()))
        print(f"[note] {len(missing)} unresolved include(s) encountered: {miss_list}", file=sys.stderr)
        print("       Add -I include paths or --follow-angle if appropriate.", file=sys.stderr)
    if unresolved:
        print(f"[note] {sum(unresolved.values())} RTTI reference(s) had no ID match in IDs_RTTI.h.", file=sys.stderr)
        print("       If your IDs file uses a different naming scheme, tweak rtti_to_symbol_candidates().", file=sys.stderr)

if __name__ == "__main__":
    main()
