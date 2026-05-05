import time
import re
import gc
import statistics
import platform
import sys
import os
import subprocess
import json
import numpy as np
import matplotlib.pyplot as plt
import mim_py as mim
from pathlib import Path
from mim_py.mim_regex import MimRegex, RegBuilder
from mim_py.regex_plug import regex
from datetime import datetime



WARMUP = 10_000
ITERATIONS = 100_000
BULK_SIZE = 10_000_000
CONFIDENCE = 0.95 #remove this, pretty useless for these plots and benchmarks       
SEED = 42                  




def collect_system_info():
    """Collect and print system information for reproducibility."""
    info = {
        "timestamp":    datetime.now().isoformat(),
        "python":       sys.version,
        "platform":     platform.platform(),
        "processor":    platform.processor(),
        "machine":      platform.machine(),
        "cpu_count":    os.cpu_count(),
    }

    try:
        with open("/proc/cpuinfo") as f:
            for line in f:
                if line.startswith("model name"):
                    info["cpu_model"] = line.split(":")[1].strip()
                    break
    except FileNotFoundError:
        info["cpu_model"] = platform.processor()

    try:
        with open("/proc/meminfo") as f:
            for line in f:
                if line.startswith("MemTotal"):
                    kb = int(line.split()[1])
                    info["ram_gb"] = round(kb / 1024 / 1024, 1)
                    break
    except FileNotFoundError:
        info["ram_gb"] = "unknown"

    for cmd, key in [("clang --version", "clang"), ("gcc --version", "gcc")]:
        try:
            out = subprocess.check_output(cmd.split(), stderr=subprocess.STDOUT, text=True)
            info[key] = out.splitlines()[0]
        except Exception:
            info[key] = "not found"

    try:
        out = subprocess.check_output(["llvm-config", "--version"], text=True).strip()
        info["llvm"] = out
    except Exception:
        info["llvm"] = "unknown"

    info["mim_py_version"] = getattr(mim, "__version__", "unknown")

    print("=" * 60)
    print("  SYSTEM INFORMATION")
    print("=" * 60)
    for k, v in info.items():
        print(f"  {k:<16}: {v}")
    print("=" * 60)
    print()

    return info


system_info = collect_system_info()


#Benchmark helper 

def confidence_interval(data, confidence=CONFIDENCE):
    """Compute mean and CI half-width using t-distribution."""
    n = len(data)
    mean = statistics.mean(data)
    if n < 2:
        return mean, 0
    se = statistics.stdev(data) / (n ** 0.5)
    # For large n (100k), t ~ z ~ 1.96 for 95%
    from scipy import stats as sp_stats
    t_crit = sp_stats.t.ppf((1 + confidence) / 2, df=n - 1)
    return mean, t_crit * se


def bench(fn, arg, warmup=WARMUP, iterations=ITERATIONS):
    for _ in range(warmup):
        fn(arg)

    gc.disable()
    times = []
    for _ in range(iterations):
        start = time.perf_counter_ns()
        fn(arg)
        times.append(time.perf_counter_ns() - start)
    gc.enable()

    mean, ci_half = confidence_interval(times)

    return {
        "times":     times,
        "median_ns": statistics.median(times),
        "mean_ns":   mean,
        "stdev_ns":  statistics.stdev(times),
        "ci_ns":     ci_half,
        "min_ns":    min(times),
        "p5_ns":     sorted(times)[int(iterations * 0.05)],
        "p95_ns":    sorted(times)[int(iterations * 0.95)],
    }


def bench_no_arg(fn, warmup=WARMUP, iterations=ITERATIONS):
    for _ in range(warmup):
        fn()
    gc.disable()
    times = []
    for _ in range(iterations):
        start = time.perf_counter_ns()
        fn()
        times.append(time.perf_counter_ns() - start)
    gc.enable()

    mean, ci_half = confidence_interval(times)

    return {
        "times":     times,
        "median_ns": statistics.median(times),
        "mean_ns":   mean,
        "ci_ns":     ci_half,
        "min_ns":    min(times),
    }


#build MimRegex

d = mim.Driver()
d.add_search_path(Path("../build/lib/mim/"))
b = RegBuilder(d, "regex", mim.Level.Warn)

alpha = b.range('a', 'z') | b.range('A', 'Z')
under_hyph = b.lit("_") | b.lit("-")
dot_under_hyph = b.lit(".") | b.lit("_") | b.lit("-")

local = (b.alnum()
         + (b.alnum()["*"] + dot_under_hyph["+"] + b.alnum())["*"]
         + b.alnum()["*"])

domain = (b.alnum()
          + (b.alnum()["*"] + under_hyph["+"] + b.alnum())["*"]
          + b.alnum()["*"])

subdomain = ((b.alnum()["*"] + under_hyph["+"] + b.alnum())["*"]
             + b.alnum()["+"]
             + b.lit("."))["*"]

tld = alpha + alpha["+"]

email = local + b.lit("@") + domain + b.lit(".") + subdomain + tld



#comp time bench
print("-- Compilation time --")

#measure the full JIT pipeline (IR -> optimize -> LLVM emit -> clang -> dlopen)
compile_times_mim = []
NUM_COMPILE_RUNS = 10  

for i in range(NUM_COMPILE_RUNS):
    # Rebuild from scratch each time to measure cold compilation
    d_tmp = mim.Driver()
    d_tmp.add_search_path(Path("../build/lib/mim/"))
    b_tmp = RegBuilder(d_tmp, "regex", mim.Level.Warn)

    alpha_t = b_tmp.range('a', 'z') | b_tmp.range('A', 'Z')
    under_hyph_t = b_tmp.lit("_") | b_tmp.lit("-")
    dot_under_hyph_t = b_tmp.lit(".") | b_tmp.lit("_") | b_tmp.lit("-")

    local_t = (b_tmp.alnum()
               + (b_tmp.alnum()["*"] + dot_under_hyph_t["+"] + b_tmp.alnum())["*"]
               + b_tmp.alnum()["*"])
    domain_t = (b_tmp.alnum()
                + (b_tmp.alnum()["*"] + under_hyph_t["+"] + b_tmp.alnum())["*"]
                + b_tmp.alnum()["*"])
    subdomain_t = ((b_tmp.alnum()["*"] + under_hyph_t["+"] + b_tmp.alnum())["*"]
                   + b_tmp.alnum()["+"]
                   + b_tmp.lit("."))["*"]
    tld_t = alpha_t + alpha_t["+"]
    email_t = local_t + b_tmp.lit("@") + domain_t + b_tmp.lit(".") + subdomain_t + tld_t

    start = time.perf_counter_ns()
    lib_tmp = email_t.jit()
    elapsed = time.perf_counter_ns() - start
    compile_times_mim.append(elapsed)
    del lib_tmp, d_tmp, b_tmp

#python re
compile_times_re = []
COMPILE_ITERATIONS_RE = 10_000

for _ in range(COMPILE_ITERATIONS_RE):
    start = time.perf_counter_ns()
    _ = re.compile(
        r"[a-zA-Z0-9](?:[a-zA-Z0-9]*[._\-]+[a-zA-Z0-9])*[a-zA-Z0-9]*"
        r"@"
        r"[a-zA-Z0-9](?:[a-zA-Z0-9]*[_\-]+[a-zA-Z0-9])*[a-zA-Z0-9]*"
        r"\."
        r"(?:(?:[a-zA-Z0-9]*[_\-]+[a-zA-Z0-9])*[a-zA-Z0-9]+\.)*"
        r"[a-zA-Z][a-zA-Z]+"
    )
    compile_times_re.append(time.perf_counter_ns() - start)

mim_compile_median_ms = statistics.median(compile_times_mim) / 1e6
re_compile_median_us = statistics.median(compile_times_re) / 1e3

print(f"  MimIR JIT (full pipeline):  {mim_compile_median_ms:.1f} ms median  "
      f"(n={NUM_COMPILE_RUNS}, stdev={statistics.stdev(compile_times_mim)/1e6:.1f} ms)")
print(f"  Python re.compile():        {re_compile_median_us:.1f} us median  "
      f"(n={COMPILE_ITERATIONS_RE})")
print(f"  Ratio: MimIR is ~{mim_compile_median_ms*1000/re_compile_median_us:.0f}x slower to compile")
print(f"  -> MimIR compilation is a one-time cost amortized over all subsequent matches")
print()


#jit relevant version that is used fuhter 
library = email.jit()
mim_match = library["match_func"]


#python re pattern 

py_pattern = re.compile(
    r"[a-zA-Z0-9](?:[a-zA-Z0-9]*[._\-]+[a-zA-Z0-9])*[a-zA-Z0-9]*"
    r"@"
    r"[a-zA-Z0-9](?:[a-zA-Z0-9]*[_\-]+[a-zA-Z0-9])*[a-zA-Z0-9]*"
    r"\."
    r"(?:(?:[a-zA-Z0-9]*[_\-]+[a-zA-Z0-9])*[a-zA-Z0-9]+\.)*"
    r"[a-zA-Z][a-zA-Z]+"
)



def run_mim(s):
    return mim_match(s.encode("utf-8"))

def run_re(s):
    return py_pattern.fullmatch(s) is not None



test_cases = {
    "Simpel & Valide":         "user@example.com",
    ". in Local":              "first.last@example.com",
    "Komplex & Valide":        "user.name-tag@sub-domain.example.co.uk",
    "Lang & Valide":           "a" * 50 + "@" + "b" * 50 + ".com",
    "Extrem Lang & Valide":    "a" * 20_000 + "@" + "b" * 200 + ".com",
    "Simpel & Invalide":       "@missing-local.com",
    "Keine TLD":               "user@example",
    "Nur Sonder":              "user!#$%@example.com",
    "Lang & Invalide":         "a" * 200 + "@" + "b" * 200,
}



print("-- Correctness check --")
for name, input_str in test_cases.items():
    mim_result = run_mim(input_str)
    re_result = run_re(input_str)
    status = "OK" if mim_result == re_result else "MISMATCH"
    print(f"  {name:<26} mim={mim_result!s:<6} re={re_result!s:<6} [{status}]")
    assert mim_result == re_result, f"MISMATCH on {name!r}"
print()



print("-- FFI overhead isolation --")

ffi_test_cases = {
    "short":      "user@example.com",
    "medium":     "user.name-tag@sub-domain.example.co.uk",
    "long":       "a" * 200 + "@" + "b" * 200 + ".com",
}

ffi_stats = {}

for name, s in ffi_test_cases.items():
    pre_encoded = s.encode("utf-8")

    encode_s     = bench_no_arg(lambda: s.encode("utf-8"))
    ctypes_match = bench(mim_match, pre_encoded)
    combined     = bench(run_mim, s)
    re_baseline  = bench(run_re, s)

    ffi_stats[name] = {
        "encode":       encode_s["median_ns"],
        "encode_ci":    encode_s["ci_ns"],
        "ctypes":       ctypes_match["median_ns"],
        "ctypes_ci":    ctypes_match["ci_ns"],
        "ctypes_stdev": ctypes_match["stdev_ns"],
        "combined":     combined["median_ns"],
        "combined_ci":  combined["ci_ns"],
        "combined_stdev": combined["stdev_ns"],
        "re":           re_baseline["median_ns"],
        "re_ci":        re_baseline["ci_ns"],
        "re_stdev":     re_baseline["stdev_ns"],
        "input_len":    len(s),
    }

    print(f"  {name} ({len(s)} chars):")
    print(f"    encode only:    {encode_s['median_ns']:>8.0f} ns  (+/-{encode_s['ci_ns']:.0f})")
    print(f"    ctypes + match: {ctypes_match['median_ns']:>8.0f} ns  (+/-{ctypes_match['ci_ns']:.0f})")
    print(f"    combined:       {combined['median_ns']:>8.0f} ns  (+/-{combined['ci_ns']:.0f})")
    print(f"    re baseline:    {re_baseline['median_ns']:>8.0f} ns  (+/-{re_baseline['ci_ns']:.0f})")
    print(f"    - encode overhead:  ~{encode_s['median_ns']:.0f} ns "
          f"({encode_s['median_ns']/combined['median_ns']*100:.0f}% of total)")
    print(f"    - ctypes+match:     ~{ctypes_match['median_ns']:.0f} ns "
          f"({ctypes_match['median_ns']/combined['median_ns']*100:.0f}% of total)")
    print()
print()



print(f"{'Case':<26} {'Len':>6}  {'Match?':>6}  "
      f"{'mim median':>12} {'+-CI':>8} {'re median':>12} {'+-CI':>8} {'speedup':>8}")
print("-" * 100)

match_stats = {}
for name, input_str in test_cases.items():
    match_stats[name] = {}
    for label, fn in [("mim", run_mim), ("re", run_re)]:
        match_stats[name][label] = bench(fn, input_str)

    mim_med = match_stats[name]["mim"]["median_ns"]
    re_med = match_stats[name]["re"]["median_ns"]
    mim_ci = match_stats[name]["mim"]["ci_ns"]
    re_ci = match_stats[name]["re"]["ci_ns"]
    speedup = re_med / mim_med

    print(f"{name:<26} {len(input_str):>6}  {str(run_mim(input_str)):>6}  "
          f"{mim_med:>10.0f}ns {mim_ci:>6.0f}  {re_med:>10.0f}ns {re_ci:>6.0f}  {speedup:>7.2f}x")
print()



print("-- Bulk throughput --")

# emails with uniform distr
rng = np.random.default_rng(SEED)

def generate_bulk_emails(n, rng):
    """Generate n emails with uniformly distributed lengths."""
    emails = []
    for _ in range(n):
        local_len = rng.integers(1, 30)
        domain_len = rng.integers(1, 20)
        local = ''.join(rng.choice(list('abcdefghijklmnopqrstuvwxyz0123456789'), local_len))
        domain = ''.join(rng.choice(list('abcdefghijklmnopqrstuvwxyz0123456789'), domain_len))
        tld = ''.join(rng.choice(list('abcdefghijklmnopqrstuvwxyz'), rng.integers(2, 5)))
        emails.append(f"{local}@{domain}.{tld}")
    return emails

print(f"  Generating {BULK_SIZE:,} emails with uniform random lengths (seed={SEED})...")
bulk_input = generate_bulk_emails(BULK_SIZE, rng)
bulk_lengths = [len(e) for e in bulk_input]
print(f"  Length distribution: min={min(bulk_lengths)}, max={max(bulk_lengths)}, "
      f"mean={np.mean(bulk_lengths):.1f}, median={np.median(bulk_lengths):.1f}")

bulk_stats = {}

for label, fn in [("mim", run_mim), ("re", run_re)]:
    #warmup
    for s in bulk_input[:1000]:
        fn(s)

    gc.disable()
    start = time.perf_counter()
    for s in bulk_input:
        fn(s)
    elapsed = time.perf_counter() - start
    gc.enable()

    bulk_stats[label] = {
        "total_ms": elapsed * 1000,
        "per_sec": len(bulk_input) / elapsed,
    }
    print(f"  {label:>4}: {len(bulk_input):,} matches in {elapsed*1000:.1f} ms "
          f"({len(bulk_input)/elapsed:,.0f} matches/sec)")
print()



raw_data = {
    "system_info": system_info,
    "config": {
        "warmup": WARMUP,
        "iterations": ITERATIONS,
        "bulk_size": BULK_SIZE,
        "seed": SEED,
    },
    "compilation": {
        "mim_jit_times_ns": compile_times_mim,
        "re_compile_times_ns": compile_times_re,
    },
    "ffi_stats": {k: {kk: vv for kk, vv in v.items()} for k, v in ffi_stats.items()},
    "match_stats": {
        name: {
            label: {k: v for k, v in stats.items() if k != "times"}
            for label, stats in engines.items()
        }
        for name, engines in match_stats.items()
    },
    "bulk_stats": bulk_stats,
}

with open("benchmark_results.json", "w") as f:
    json.dump(raw_data, f, indent=2, default=str)
print("Raw data saved to benchmark_results.json")
print()



plt.style.use("seaborn-v0_8-whitegrid")
plt.rcParams.update({"font.size": 11})


#FFI overhead breakdown 

ffi_names = list(ffi_stats.keys())
ffi_labels = [f"{n}\n({ffi_stats[n]['input_len']} chars)" for n in ffi_names]
x = np.arange(len(ffi_names))
width = 0.2

fig, ax = plt.subplots(figsize=(10, 5))

encode_vals  = [ffi_stats[n]["encode"] / 1000 for n in ffi_names]
ctypes_vals  = [ffi_stats[n]["ctypes"] / 1000 for n in ffi_names]
combined_vals = [ffi_stats[n]["combined"] / 1000 for n in ffi_names]
re_vals      = [ffi_stats[n]["re"] / 1000 for n in ffi_names]

encode_err   = [ffi_stats[n]["encode_ci"] / 1000 for n in ffi_names]
ctypes_err   = [ffi_stats[n]["ctypes_ci"] / 1000 for n in ffi_names]
combined_err = [ffi_stats[n]["combined_ci"] / 1000 for n in ffi_names]
re_err       = [ffi_stats[n]["re_ci"] / 1000 for n in ffi_names]

ax.bar(x - width * 1.5, encode_vals, width, yerr=encode_err,
       label="encode only", color="gold", zorder=3, capsize=3)
ax.bar(x - width * 0.5, ctypes_vals, width, yerr=ctypes_err,
       label="ctypes + match\n(pre-encoded)", color="steelblue", zorder=3, capsize=3)
ax.bar(x + width * 0.5, combined_vals, width, yerr=combined_err,
       label="encode + ctypes + match\n(combined)", color="mediumpurple", zorder=3, capsize=3)
ax.bar(x + width * 1.5, re_vals, width, yerr=re_err,
       label="Python re", color="coral", zorder=3, capsize=3)

ax.set_xticks(x)
ax.set_xticklabels(ffi_labels)
ax.set_ylabel("Median time (us)")
ax.set_title("FFI Overhead Isolation")
ax.legend(loc="upper left")
ax.grid(axis="y", alpha=0.3)
fig.tight_layout()
fig.savefig("01_ffi_overhead.png", dpi=150)


#median match time

cases = list(test_cases.keys())
mim_medians = [match_stats[c]["mim"]["median_ns"] / 1000 for c in cases]
re_medians  = [match_stats[c]["re"]["median_ns"] / 1000 for c in cases]
mim_errs    = [match_stats[c]["mim"]["ci_ns"] / 1000 for c in cases]
re_errs     = [match_stats[c]["re"]["ci_ns"] / 1000 for c in cases]

x = np.arange(len(cases))
width = 0.35

fig, ax = plt.subplots(figsize=(14, 5))
ax.bar(x - width / 2, mim_medians, width, yerr=mim_errs,
       label="mim", zorder=3, capsize=3)
ax.bar(x + width / 2, re_medians, width, yerr=re_errs,
       label="re", zorder=3, capsize=3)
ax.set_xticks(x)
ax.set_xticklabels(cases, rotation=35, ha="right")
ax.set_ylabel("Median time (us)")
ax.set_yscale("log")
ax.set_title("Match Latency by Test Case (log scale)")
ax.legend()
ax.grid(axis="y", alpha=0.3)
fig.tight_layout()
fig.savefig("02_match_latency.png", dpi=150)


#speedup/case 

speedups = [re_medians[i] / mim_medians[i] for i in range(len(cases))]

fig, ax = plt.subplots(figsize=(12, 4))
colors = ["green" if s > 1 else "red" for s in speedups]
bars = ax.bar(cases, speedups, color=colors, edgecolor="black", linewidth=0.5, zorder=3)
ax.axhline(1.0, color="black", linewidth=1, linestyle="--")
ax.set_yscale("log", base=2)
ax.set_ylabel("Speedup (re / mim) -- log2 scale")
ax.set_title("mim Speedup Over Python re (>1 = mim faster)")
ax.set_xticklabels(cases, rotation=35, ha="right")
ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda v, _: f"{v:.2g}x"))
from matplotlib.patches import Patch
ax.legend(handles=[Patch(color="green", label="mim faster"),
                   Patch(color="red", label="re faster")],
          loc="upper left")
ax.grid(axis="y", alpha=0.3)
fig.tight_layout()
fig.savefig("03_speedup.png", dpi=150)


#latency dist

highlight_cases = [
    "Simpel & Valide", "Komplex & Valide",
    "Extrem Lang & Valide", "Lang & Invalide",
]

fig, axes = plt.subplots(2, 2, figsize=(12, 8))
for ax, case in zip(axes.flat, highlight_cases):
    mim_us = np.array(match_stats[case]["mim"]["times"]) / 1000
    re_us  = np.array(match_stats[case]["re"]["times"]) / 1000

    lo = min(np.percentile(mim_us, 1), np.percentile(re_us, 1))
    hi = max(np.percentile(mim_us, 99), np.percentile(re_us, 99))
    bins = np.linspace(lo, hi, 60)

    ax.hist(mim_us, bins=bins, alpha=0.6, label="mim", edgecolor="black", linewidth=0.3)
    ax.hist(re_us,  bins=bins, alpha=0.6, label="re",  edgecolor="black", linewidth=0.3)

    # Add median markers
    ax.axvline(np.median(mim_us), color="tab:blue", linestyle="--", alpha=0.8, linewidth=1.2)
    ax.axvline(np.median(re_us),  color="tab:orange", linestyle="--", alpha=0.8, linewidth=1.2)

    ax.set_title(case)
    ax.set_xlabel("Time (us)")
    ax.set_ylabel("Frequency")
    ax.legend()

fig.suptitle("Match Latency Distributions (with median markers)", fontweight="bold")
fig.tight_layout()
fig.savefig("04_latency_distributions.png", dpi=150)


#bulk throughput

fig, ax = plt.subplots(figsize=(6, 4))
labels = list(bulk_stats.keys())
rates = [bulk_stats[l]["per_sec"] for l in labels]
ax.bar(labels, rates, color=["steelblue", "coral"],
       edgecolor="black", linewidth=0.5, zorder=3)
ax.set_ylabel("Matches / sec")
ax.set_title(f"Bulk Throughput ({BULK_SIZE:,} emails, uniform length distribution)")
ax.grid(axis="y", alpha=0.3)
for i, v in enumerate(rates):
    ax.text(i, v * 1.02, f"{v:,.0f}", ha="center", fontweight="bold")
fig.tight_layout()
fig.savefig("05_bulk_throughput.png", dpi=150)


#comp time

fig, ax = plt.subplots(figsize=(7, 5))

labels = ["MimIR JIT", "re.compile()"]
# Both values in microseconds for a common unit
times_us = [mim_compile_median_ms * 1000, re_compile_median_us]
colors = ["steelblue", "coral"]

bars = ax.bar(labels, times_us, color=colors,
              edgecolor="black", linewidth=0.5, zorder=3, width=0.5)

ax.set_yscale("log")
ax.set_ylabel("Zeit (µs, log-Skala)")
ax.set_title("Kompilierungszeit: MimIR JIT vs. re.compile()")
ax.grid(axis="y", alpha=0.3)

for bar, val in zip(bars, times_us):
    if val >= 1000:
        label = f"{val/1000:.1f} ms"
    else:
        label = f"{val:.1f} µs"
    ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() * 1.3,
            label, ha="center", fontweight="bold", fontsize=12)

ax.set_ylim(top=ax.get_ylim()[1] * 5)

fig.tight_layout()
fig.savefig("06_compilation_time.png", dpi=150)