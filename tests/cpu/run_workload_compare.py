#!/usr/bin/env python3
"""Build immutable CPU source snapshots and compare checked, fixed-work guests.

Only standard-library Python, CMake, Ninja and GNU-compatible x86-64 compilers
are needed. Revisions are exported without checking out or modifying a branch.
Positive time_delta_pct means slower; positive throughput_delta_pct means faster.
"""
import argparse
import csv
import hashlib
import io
import itertools
import json
import math
import os
from pathlib import Path, PurePosixPath
import platform
import random
import shutil
import statistics as stats
import subprocess
import tarfile
import time

SOURCE_DIRS = ("src/cpu", "src/codegen", "src/mem", "src/include")
FIXTURES = ("workload_bench.c", "full_core.cmake", "full_core_platform.c")
# Build wiring may add CPU source files between revisions. The executable guest
# and platform must be identical; all build inputs are still hashed separately.
SEMANTIC_FIXTURES = ("workload_bench.c", "full_core_platform.c")
FLAGS = "-fomit-frame-pointer -fno-strict-aliasing -m64 -march=x86-64 -msse2 -mfpmath=sse -mstackrealign"
CMAKE = """cmake_minimum_required(VERSION 3.20)
project(pcbox_workload_bench LANGUAGES C CXX)
get_filename_component(CPU_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
include(full_core.cmake)
add_executable(cpu_workload_bench workload_bench.c)
pcbox_full_cpu_target(cpu_workload_bench)
"""


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def dump(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def write_csv(path, rows):
    if not rows:
        return
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def logged(command, log, env):
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, env=env)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}); see {log}")


def build(repo, output, label, ref, args, env):
    commit = subprocess.check_output(["git", "-C", str(repo), "rev-parse",
                                      f"{'HEAD' if ref == 'worktree' else ref}^{{commit}}"], text=True).strip()
    root = output / label
    source = root / "source"
    source.mkdir(parents=True, exist_ok=True)
    if any(source.iterdir()):
        raise ValueError(f"Source snapshot already exists: {source}")
    archive = subprocess.check_output(["git", "-C", str(repo), "archive", commit, *SOURCE_DIRS])
    with tarfile.open(fileobj=io.BytesIO(archive)) as stream:
        # Do not resolve Windows ancestors outside the workspace. These are
        # regular Git source files; reject links and unsafe member names.
        for member in stream:
            path = PurePosixPath(member.name)
            if path.is_absolute() or ".." in path.parts or ":" in member.name or "\\" in member.name:
                raise ValueError(f"Unsafe archive path: {member.name}")
            target = source.joinpath(*path.parts)
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(stream.extractfile(member).read())
            else:
                raise ValueError(f"Unsupported source archive entry: {member.name}")
    if ref == "worktree":
        # Copy tracked working files, including local deletions, over HEAD.
        tracked = subprocess.check_output(["git", "-C", str(repo), "ls-files", "-z", "--", *SOURCE_DIRS])
        for name in tracked.decode().split("\0"):
            if not name:
                continue
            target = source / name
            if (repo / name).exists():
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(repo / name, target)
            elif target.exists():
                target.unlink()
    fixture_dir = source / "tests/cpu"
    fixture_dir.mkdir(parents=True)
    for name in FIXTURES:
        shutil.copy2(repo / "tests/cpu" / name, fixture_dir / name)
    (fixture_dir / "CMakeLists.txt").write_text(CMAKE, encoding="utf-8")
    source_hashes = {str(p.relative_to(source)): sha(p) for p in sorted(source.rglob("*")) if p.is_file()}
    configure = [args.cmake, "-S", str(fixture_dir), "-B", str(root / "build"), "-G", "Ninja",
                 "-DCMAKE_BUILD_TYPE=RelWithDebInfo", f"-DCMAKE_C_COMPILER={args.cc}",
                 f"-DCMAKE_CXX_COMPILER={args.cxx}", f"-DCMAKE_C_FLAGS={FLAGS}", f"-DCMAKE_CXX_FLAGS={FLAGS}"]
    logged(configure, root / "configure.log", env)
    logged([args.cmake, "--build", str(root / "build"), "--target", "cpu_workload_bench", "-j", str(args.jobs)],
           root / "build.log", env)
    executable = root / "build" / ("cpu_workload_bench.exe" if os.name == "nt" else "cpu_workload_bench")
    info = dict(ref=ref, commit=commit, configure=configure, source_sha256=source_hashes,
                executable=str(executable), executable_sha256=sha(executable))
    if ref == "worktree":
        info["working_diff"] = subprocess.check_output(["git", "-C", str(repo), "diff", "HEAD", "--", *SOURCE_DIRS], text=True)
    dump(root / "build.json", info)
    print(f"Built {label}: {commit[:12]}{' + local changes' if ref == 'worktree' else ''}", flush=True)
    return info


def samples(path, name, count):
    with path.open(encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    if len(rows) != count or any(r["case"] != name for r in rows):
        raise ValueError(f"Unexpected samples: {path}")
    for i, r in enumerate(rows):
        ns = float(r["ns_per_call"])
        if (int(r["sample"]) != i or int(r["calls"]) <= 0 or int(r["calls"]) % 256
                or not math.isfinite(ns) or ns <= 0
                or not math.isclose(float(r["elapsed_ns"]) / int(r["calls"]), ns, abs_tol=1e-5)):
            raise ValueError(f"Invalid timing sample: {path}")
    return rows


def summarize(output, names, labels, rounds, collected):
    summaries, comparisons = [], []
    for name in names:
        valid = {}
        for label in labels:
            runs = collected.get((name, label), [])
            if len(runs) != rounds:
                continue
            values = [float(r["ns_per_call"]) for run in runs for r in run]
            median = stats.median(values)
            valid[label] = dict(median=median, mad=stats.median(abs(x - median) for x in values),
                                cv=100 * stats.stdev(values) / stats.mean(values),
                                rounds=[stats.median(float(r["ns_per_call"]) for r in run) for run in runs],
                                signature={(r["kernel_iterations"], r["checksum"]) for run in runs for r in run})
            summaries.append(dict(case=name, version=label, median_ns=median, cv_pct=valid[label]["cv"],
                                  mad_ns=valid[label]["mad"], samples=len(values), round_medians=json.dumps(valid[label]["rounds"])))
        for before, after in itertools.combinations(labels, 2):
            if before not in valid or after not in valid:
                continue
            a, b = valid[before], valid[after]
            if len(a["signature"]) != 1 or a["signature"] != b["signature"]:
                raise ValueError(f"Guest work/results differ: {name} {before} {after}")
            delta = 100 * (b["median"] / a["median"] - 1)
            deltas = [100 * (y / x - 1) for x, y in zip(a["rounds"], b["rounds"])]
            threshold = max(3, 300 * math.hypot(a["mad"] / a["median"], b["mad"] / b["median"]))
            decision = "no_clear_change"
            if max(a["cv"], b["cv"]) > 5:
                decision = "noisy"
            elif rounds < 2:
                decision = "needs_repeat"
            elif delta > threshold and all(d > 3 for d in deltas):
                decision = "regression"
            elif delta < -threshold and all(d < -3 for d in deltas):
                decision = "gain"
            elif delta > 3:
                # An estimated slowdown over the budget is not a pass just
                # because noise prevents confirming it in every round.
                decision = "needs_repeat"
            comparisons.append(dict(case=name, before=before, after=after, baseline_ns=a["median"], current_ns=b["median"],
                                    time_delta_pct=delta, throughput_delta_pct=100 * (a["median"] / b["median"] - 1),
                                    decision=decision, baseline_cv_pct=a["cv"], current_cv_pct=b["cv"],
                                    threshold_pct=threshold, round_deltas_pct=json.dumps(deltas)))
    write_csv(output / "summary.csv", summaries)
    write_csv(output / "comparison.csv", comparisons)
    aggregates = []
    for before, after in itertools.combinations(labels, 2):
        rows = [r for r in comparisons if r["before"] == before and r["after"] == after]
        if rows:
            speedup = math.exp(stats.mean(math.log(r["baseline_ns"] / r["current_ns"]) for r in rows))
            aggregates.append(dict(before=before, after=after, cases=len(rows), geometric_mean_speedup=speedup,
                                   throughput_delta_pct=100 * (speedup - 1),
                                   decisions={d: sum(r["decision"] == d for r in rows) for d in sorted({r["decision"] for r in rows})}))
    dump(output / "aggregate.json", aggregates)
    print(json.dumps(aggregates, indent=2), flush=True)
    return comparisons


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--variant", action="append", help="LABEL=REVISION, or LABEL=worktree; repeat in comparison order")
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--cpu", type=int)
    parser.add_argument("--rounds", type=int, default=6)
    parser.add_argument("--samples", type=int, default=9)
    parser.add_argument("--sample-ms", type=int, default=75)
    parser.add_argument("--filter", default="")
    parser.add_argument("--interpreter", action="store_true", help="Measure the interpreter instead of the dynamic recompiler")
    parser.add_argument("--seed", type=int, default=20261012)
    parser.add_argument("--build-only", action="store_true")
    parser.add_argument("--reuse-builds", action="store_true", help="Reuse hash-checked builds and build missing variants")
    parser.add_argument("--run-name", default="", help="Save a follow-up under this simple directory name without overwriting earlier timings")
    parser.add_argument("--fail-on-regression", action="store_true",
                        help="Fail on incorrect guests or repeatable slowdowns; inconclusive/noisy measurements exit 2")
    args = parser.parse_args()
    if not (args.rounds >= 1 and 2 <= args.samples <= 99 and 1 <= args.sample_ms <= 1000 and args.jobs > 0):
        parser.error("Invalid measurement/build settings")
    if args.fail_on_regression and args.rounds < 3:
        parser.error("A performance gate needs at least three paired rounds")
    if args.run_name and not all(c.isalnum() or c in "_-" for c in args.run_name):
        parser.error("Use a simple run-name containing letters, digits, underscores or hyphens")
    repo = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    variants = [v.split("=", 1) for v in (args.variant or ["before=HEAD", "after=worktree"])]
    if any(len(v) != 2 or not v[0] or not all(c.isalnum() or c in "_-" for c in v[0]) for v in variants):
        parser.error("Use LABEL=REVISION with a simple label")
    labels = [v[0] for v in variants]
    if len(set(labels)) != len(labels) or (len(labels) < 2 and not args.build_only):
        parser.error("At least two unique variants are required for comparison (one is allowed with --build-only)")
    env = os.environ.copy()
    for attr in ("cc", "cxx", "cmake"):
        path = shutil.which(getattr(args, attr))
        if not path:
            parser.error(f"Cannot find {attr}: {getattr(args, attr)}")
        setattr(args, attr, str(Path(path).resolve()))
    env["PATH"] = str(Path(args.cc).parent) + os.pathsep + env["PATH"]
    output.mkdir(parents=True, exist_ok=True)
    builds = {}
    for label, ref in variants:
        if args.reuse_builds and (output / label / "build.json").exists():
            info = json.loads((output / label / "build.json").read_text())
            if info["ref"] != ref or sha(Path(info["executable"])) != info["executable_sha256"]:
                raise ValueError(f"Build identity changed: {label}")
            builds[label] = info
        else:
            builds[label] = build(repo, output, label, ref, args, env)
    if args.build_only:
        return
    fixture_hashes = []
    for info in builds.values():
        hashes = {k.replace("\\", "/"): v for k, v in info["source_sha256"].items()}
        fixture_hashes.append({name: hashes[f"tests/cpu/{name}"] for name in SEMANTIC_FIXTURES})
    if any(h != fixture_hashes[0] for h in fixture_hashes):
        raise ValueError("Builds used different fixtures; rebuild in a fresh output directory")
    if args.run_name:
        output = output / args.run_name
        output.mkdir(parents=True, exist_ok=True)
    runs = output / "runs"
    runs.mkdir()  # Refuse to mix or overwrite previous timings.
    inventories = {label: subprocess.check_output([b["executable"], "--list"], text=True, env=env).splitlines()
                   for label, b in builds.items()}
    if any(v != inventories[labels[0]] for v in inventories.values()):
        raise ValueError("Case inventories differ")
    names = [n for n in inventories[labels[0]] if args.filter in n]
    if not names:
        raise ValueError("No selected workloads")
    manifest = dict(settings=vars(args) | {"output": str(output)}, host=platform.platform(), logical_cpus=os.cpu_count(),
                    compiler=subprocess.check_output([args.cc, "--version"], text=True, env=env).splitlines()[0],
                    variants={label: {k: b[k] for k in ("ref", "commit", "executable", "executable_sha256")} for label, b in builds.items()},
                    fixture_sha256=fixture_hashes[0], runner_sha256=sha(Path(__file__)), cases=names)
    dump(output / "manifest.json", manifest)
    failures, collected, observations = [], {}, []
    invalid = set()
    affinity = ["--cpu", str(args.cpu)] if args.cpu is not None else []
    # Validate both engines before timing; invalid historical results never count as gains.
    for index, name in enumerate(names):
        for label in labels:
            for engine in ("interpreter", "dynarec"):
                command = [builds[label]["executable"], "--case", name, "--validate-only", *affinity]
                if engine == "interpreter":
                    command.append("--interpreter")
                log = runs / f"{index:02d}-{label}-{engine}-validation.log"
                with log.open("w", encoding="utf-8") as stream:
                    p = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, env=env, timeout=120)
                if p.returncode:
                    failures.append(dict(case=name, version=label, engine=engine, exit_code=p.returncode, log=str(log)))
                    invalid.add((name, label))
    dump(output / "validation_failures.json", failures)
    print(f"Validation complete: {len(invalid)} invalid case/version combinations", flush=True)
    rng = random.Random(args.seed)
    started = time.monotonic()
    for round_index in range(args.rounds):
        order = list(enumerate(names)); rng.shuffle(order)
        for index, name in order:
            shift = (round_index + index) % len(labels)
            versions = labels[shift:] + labels[:shift]
            if (round_index // len(labels)) % 2:
                versions.reverse()
            for label in versions:
                if (name, label) in invalid:
                    continue
                stem = runs / f"{index:02d}-r{round_index + 1}-{label}"
                command = [builds[label]["executable"], "--case", name, "--samples", str(args.samples),
                           "--sample-ms", str(args.sample_ms), "--csv", str(stem.with_suffix(".csv")), *affinity]
                if args.interpreter:
                    command.append("--interpreter")
                with stem.with_suffix(".log").open("w", encoding="utf-8") as stream:
                    subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, env=env, check=True, timeout=120)
                rows = samples(stem.with_suffix(".csv"), name, args.samples)
                collected.setdefault((name, label), []).append(rows)
                observations.extend(dict(version=label, round=round_index + 1, **row) for row in rows)
        print(f"Round {round_index + 1}/{args.rounds}, {(time.monotonic() - started) / 60:.1f} min", flush=True)
    write_csv(output / "samples.csv", observations)
    comparisons = summarize(output, names, labels, args.rounds, collected)
    print(f"Results: {output / 'comparison.csv'}", flush=True)
    if args.fail_on_regression:
        if failures or any(row["decision"] == "regression" for row in comparisons):
            raise SystemExit(1)
        if not comparisons or any(row["decision"] in ("noisy", "needs_repeat") for row in comparisons):
            raise SystemExit(2)


if __name__ == "__main__":
    main()
