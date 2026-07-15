#!/usr/bin/env python3
import os
import re
import subprocess
import sys

root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
build = os.path.join(root, os.environ.get("COVERAGE_BUILD_DIR", "build-coverage"))
src_root = os.path.join(root, "src")


def collect_sources():
    sources = []
    for dirpath, _, files in os.walk(src_root):
        for name in files:
            if name.endswith(".c"):
                sources.append(os.path.join(dirpath, name))
    return sorted(sources)


def run_gcov(sources):
    os.chdir(build)
    for src in sources:
        rel = os.path.relpath(src, root).replace("\\", "/")
        parts = rel.split("/")
        # Try .o (Linux) first, then .obj (Windows)
        obj_path = os.path.join("CMakeFiles", "openspodes.dir", *parts[:-1], parts[-1] + ".o")
        if not os.path.exists(obj_path):
            obj_path = os.path.join("CMakeFiles", "openspodes.dir", *parts[:-1], parts[-1] + ".obj")
        if not os.path.exists(obj_path):
            continue
        subprocess.run(
            ["gcov", "-b", "-c", "-o", os.path.dirname(obj_path), obj_path],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )


def parse_gcov(src):
    rel = os.path.relpath(src, root).replace("\\", "/")
    gcov_path = os.path.join(build, os.path.basename(src) + ".gcov")
    if not os.path.exists(gcov_path):
        return {
            "rel": rel,
            "le": 0,
            "lt": 0,
            "be": 0,
            "bt": 0,
            "unc_lines": [],
            "unc_br": [],
        }

    lines_exec = lines_total = 0
    branches_exec = branches_total = 0
    uncovered_lines = []
    uncovered_branches = []
    current_line = None

    with open(gcov_path, "r", encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            line = raw.rstrip("\n")
            m = re.match(r"\s*(\d+|#+|\-+):\s*(\d+):\s*(.*)$", line)
            if m:
                current_line = int(m.group(2)) if m.group(2).isdigit() else None
                count = m.group(1)
                code = m.group(3)
                if count.isdigit() or count.startswith("#"):
                    lines_total += 1
                    if count.isdigit() and int(count) > 0:
                        lines_exec += 1
                    elif code.strip() and not code.strip().startswith("//"):
                        uncovered_lines.append((current_line, code.strip()[:80]))
                continue

            if line.strip().startswith("branch"):
                m = re.search(r"branch\s+\d+\s+(taken\b|never executed)", line)
                if m and current_line is not None:
                    branches_total += 1
                    if m.group(1) == "taken":
                        branches_exec += 1
                    else:
                        uncovered_branches.append((current_line, line.strip()))

    return {
        "rel": rel,
        "le": lines_exec,
        "lt": lines_total,
        "be": branches_exec,
        "bt": branches_total,
        "unc_lines": uncovered_lines,
        "unc_br": uncovered_branches,
    }


def pct(num, den):
    return 100.0 * num / den if den else 100.0


def main():
    if not os.path.isdir(build):
        print("Run coverage build first: build-coverage/", file=sys.stderr)
        return 1

    sources = collect_sources()
    run_gcov(sources)
    results = [parse_gcov(src) for src in sources]

    lt = sum(r["lt"] for r in results)
    le = sum(r["le"] for r in results)
    bt = sum(r["bt"] for r in results)
    be = sum(r["be"] for r in results)

    print("=== SUMMARY (src/) ===")
    print(f"Line coverage:   {le}/{lt} ({pct(le, lt):.1f}%)")
    print(f"Branch coverage: {be}/{bt} ({pct(be, bt):.1f}%)")
    print()

    print("=== BY MODULE ===")
    modules = {}
    for r in results:
        mod = r["rel"].split("/")[1] if "/" in r["rel"] else "root"
        m = modules.setdefault(mod, {"le": 0, "lt": 0, "be": 0, "bt": 0})
        m["le"] += r["le"]
        m["lt"] += r["lt"]
        m["be"] += r["be"]
        m["bt"] += r["bt"]

    for mod, m in sorted(modules.items(), key=lambda x: pct(x[1]["le"], x[1]["lt"])):
        print(
            f"{pct(m['le'], m['lt']):5.1f}% lines | "
            f"{pct(m['be'], m['bt']):5.1f}% branches | "
            f"{mod} ({m['le']}/{m['lt']} lines)"
        )

    print()
    print("=== FILES < 80% LINE COVERAGE ===")
    for r in sorted(results, key=lambda x: pct(x["le"], x["lt"])):
        if r["lt"] == 0:
            continue
        if pct(r["le"], r["lt"]) < 80:
            print(
                f"{pct(r['le'], r['lt']):5.1f}% lines ({r['le']}/{r['lt']}) | "
                f"{pct(r['be'], r['bt']):5.1f}% branches ({r['be']}/{r['bt']}) | "
                f"{r['rel']}"
            )

    print()
    print("=== ZERO / NEAR-ZERO COVERAGE (<20%) ===")
    for r in sorted(results, key=lambda x: pct(x["le"], x["lt"])):
        if r["lt"] and pct(r["le"], r["lt"]) < 20:
            print(f"{pct(r['le'], r['lt']):5.1f}% | {r['rel']}")

    print()
    print("=== FULLY COVERED (100% lines) ===")
    full = [r for r in results if r["lt"] > 0 and pct(r["le"], r["lt"]) == 100.0]
    if full:
        for r in sorted(full, key=lambda x: x["rel"]):
            print(f"  {r['le']}/{r['lt']} lines | {r['be']}/{r['bt']} branches | {r['rel']}")
    else:
        print("  (none)")

    print()
    print("=== UNCOVERED LINES (sample, first 30) ===")
    uncovered = []
    for r in results:
        for ln, code in r["unc_lines"]:
            uncovered.append((r["rel"], ln, code))
    for rel, ln, code in uncovered[:30]:
        print(f"  {rel}:{ln}  {code}")
    print(f"  ... total uncovered executable lines: {len(uncovered)}")

    print()
    print("=== UNTESTED BRANCHES (sample, first 30) ===")
    all_ub = []
    for r in results:
        for ln, br in r["unc_br"]:
            all_ub.append((r["rel"], ln, br))
    for rel, ln, br in all_ub[:30]:
        print(f"  {rel}:{ln}  {br}")
    print(f"  ... total untested branches: {len(all_ub)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
