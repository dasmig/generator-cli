#!/usr/bin/env python3
"""Tests for dasmig-gen: python3 tests/test_cli.py build/dasmig-gen

The known-output hashes are the SHA-256 of the exact bytes of a run. They are
the same on every platform: that is the point of the libraries' own random
engine, and the CI checks it on Linux (x86-64 and ARM64), macOS and Windows.
"""

import csv
import hashlib
import io
import json
import subprocess
import sys
import tempfile
from pathlib import Path

BIN = sys.argv[1] if len(sys.argv) > 1 else "build/dasmig-gen"
KINDS = ["persona", "name", "surname", "nickname", "city", "country", "birth", "biodata", "trait", "interest",
         "occupation", "user"]

KNOWN = {
    ("persona", "1f"): "0ce617c55264239d0ee6f77c8793eb7c536bf594949275dc8dadd569e3058eac",
    ("name", "2a"): "92523b6a46b7abfb0df42cedcac244cfccb4c5e358e8cd9dd4fb35e28f2987c9",
    ("city", "3"): "18363ff56b0229a14f7544258d3fc26e17c95d398a18683e605da11340f86aef",
    ("occupation", "4"): "a3a8e38268b29c82c38bda278d69ac1930ecabc236f5dbd61ddd0fb0f2c1a605",
    ("user", "5"): "5a721cba489e16ac45dd237ecce547b461d2e35f350dada8bec376c0b3b3e3f0",
}

failures = []


def run(*args, expect=0):
    result = subprocess.run([BIN, *args], capture_output=True)
    if result.returncode != expect:
        failures.append(f"{' '.join(args)}: exit {result.returncode}, expected {expect}: {result.stderr.decode()[:300]}")
    return result


def check(condition, message):
    if not condition:
        failures.append(message)


def lines(data):
    return data.decode("utf-8").split("\n")[:-1]


# --- help, version, usage errors ---
out = run("--version").stdout.decode()
check(out.startswith("dasmig-gen ") and "dasmig-core" in out, f"--version: {out!r}")
out = run("--help").stdout.decode()
check("Usage: dasmig-gen KIND" in out and "--threads" in out, "--help text")
for args in [[], ["nosuchkind"], ["persona", "--bogus"], ["persona", "--seed", "xyz"], ["persona", "--seed", ""],
             ["persona", "--seed", "10000000000000000"], ["persona", "--count", "-1"], ["persona", "--count"],
             ["persona", "--format", "xml"], ["persona", "--tier", "medium"], ["city", "--sex", "male"],
             ["name", "--age", "30"], ["persona", "--age", "40-20"], ["persona", "--age", "121"],
             ["persona", "--reference-date", "2023-02-30"], ["persona", "--sex", "other"],
             ["biodata", "--age", "20-30"], ["persona", "persona"],
             ["user", "--unique", "city"], ["city", "--unique", "email"], ["persona", "--safe"]]:
    result = run(*args, expect=2)
    check(b"dasmig-gen:" in result.stderr, f"usage error message for {args}")
run("persona", "--country", "XX", "--count", "1", "--seed", "1", expect=1)
run("user", "--age", "5-10", "--count", "1", "--seed", "1", expect=1)
check(run("persona", "--tier", "full", "--data-dir", tempfile.gettempdir(), "--count", "1", "--seed", "1",
          expect=1).returncode == 1, "a missing full tier is an error")

# --- every kind: lines of JSON, no carriage returns ---
for kind in KINDS:
    data = run(kind, "--count", "3", "--seed", "1").stdout
    check(b"\r" not in data, f"{kind}: carriage return in the output")
    rows = lines(data)
    check(len(rows) == 3, f"{kind}: {len(rows)} lines")
    for row in rows:
        try:
            json.loads(row)
        except ValueError as error:
            failures.append(f"{kind}: bad JSON line ({error})")
    # The unseeded call prints its seed so the run can be repeated.
    unseeded = run(kind, "--count", "2")
    seed = unseeded.stderr.decode().strip().removeprefix("seed: ")
    check(len(seed) == 16, f"{kind}: seed on stderr: {unseeded.stderr!r}")
    check(run(kind, "--count", "2", "--seed", seed).stdout == unseeded.stdout, f"{kind}: replay from the printed seed")

# --- the output does not depend on threads; ranges and splits agree ---
for kind in ("persona", "name", "city"):
    base = run(kind, "--count", "300", "--seed", "1f", "--threads", "1").stdout
    for threads in ("2", "7", "0"):
        check(run(kind, "--count", "300", "--seed", "1f", "--threads", threads).stdout == base,
              f"{kind}: threads {threads} differ")
    tail = run(kind, "--count", "100", "--first", "200", "--seed", "1f", "--threads", "3").stdout
    check(tail == b"\n".join(base.split(b"\n")[200:300]) + b"\n", f"{kind}: --first range differs")
    single = run(kind, "--count", "1", "--first", "17", "--seed", "1f").stdout
    check(single == base.split(b"\n")[17] + b"\n", f"{kind}: item 17 alone differs")

# --- formats ---
count = 25
rows = run("persona", "--count", str(count), "--seed", "5", "--format", "csv").stdout.decode("utf-8")
table = list(csv.reader(io.StringIO(rows, newline="")))
check(len(table) == count + 1 and "country.cca2" in table[0] and "signature.seed" in table[0], "csv shape")
check(all(len(row) == len(table[0]) for row in table), "csv rows have as many cells as the header")
doc = json.loads(run("persona", "--count", str(count), "--seed", "5", "--format", "json").stdout)
check(isinstance(doc, list) and len(doc) == count, "json array")
check([p["signature"]["seed"] for p in doc] == [json.loads(r)["signature"]["seed"] for r in
      lines(run("persona", "--count", str(count), "--seed", "5").stdout)], "json and jsonl agree")
check(json.loads(run("persona", "--count", "0", "--format", "json").stdout) == [], "empty json array")
check(run("persona", "--count", "0", "--format", "jsonl").stdout == b"", "empty jsonl")
check(run("persona", "--count", "0", "--format", "csv").stdout == b"", "empty csv")

with tempfile.TemporaryDirectory() as tmp:
    target = Path(tmp) / "out.jsonl"
    run("persona", "--count", "10", "--seed", "1", "--out", str(target))
    check(target.read_bytes() == run("persona", "--count", "10", "--seed", "1").stdout, "--out equals standard output")
    run("persona", "--count", "1", "--seed", "1", "--out", str(Path(tmp) / "no" / "such" / "dir.jsonl"), expect=1)

# --- requests ---
for row in lines(run("persona", "--count", "20", "--seed", "2", "--country", "br", "--sex", "female",
                     "--age", "25-35", "--traits", "5").stdout):
    p = json.loads(row)
    check(p["country"]["cca2"] == "BR" and p["sex"] == "female" and 25 <= p["age"] <= 35
          and len(p["personality"]["traits"]) <= 5, "persona request fields")
for row in lines(run("persona", "--count", "3", "--seed", "2", "--country", "JP",
                     "--reference-date", "2030-03-09").stdout):
    check(json.loads(row)["birth"]["date"] <= "2030-03-09", "persona reference date")
for row in lines(run("city", "--count", "20", "--seed", "2", "--country", "jp", "--uniform").stdout):
    check(json.loads(row)["country_code"] == "JP", "city country")
for row in lines(run("country", "--count", "20", "--seed", "2", "--region", "Europe").stdout):
    check(json.loads(row)["region"] == "Europe", "country region")
for row in lines(run("biodata", "--count", "10", "--seed", "2", "--country", "FR", "--sex", "male",
                     "--age", "40").stdout):
    check(json.loads(row)["sex"] == "male", "biodata sex")
nick = run("nickname", "--count", "1", "--seed", "1", "--name", "Ana Maria Silva").stdout
check(len(lines(nick)) == 1, "nickname from a name")
check(len(json.loads(lines(run("trait", "--count", "1", "--seed", "1", "--traits", "6").stdout)[0])["traits"]) <= 6,
      "trait count")

# --- user: the request, safe contacts and unique keys ---
for row in lines(run("user", "--count", "20", "--seed", "2", "--country", "br", "--sex", "female",
                     "--age", "25-35").stdout):
    u = json.loads(row)
    check(u["country"]["code"] == "BR" and u["sex"] == "female" and 25 <= u["age"] <= 35
          and u["address"]["country_code"] == "BR" and u["phone"]["country_code"] == "BR", "user request fields")
safe = [json.loads(r) for r in lines(run("user", "--count", "30", "--seed", "4", "--safe").stdout)]
check(len(safe) == 30 and all(u["email"]["reserved"] for u in safe), "user --safe")
for key, field in (("email", lambda u: u["email"]["address"]), ("username", lambda u: u["account"]["username"])):
    batch = [json.loads(r) for r in lines(run("user", "--count", "2000", "--seed", "7", "--country", "IS",
                                              "--unique", key, "--threads", "3").stdout)]
    check(len(batch) == 2000 and len({field(u) for u in batch}) == 2000, f"user unique {key}")

# --- known outputs, the same on every platform ---
for (kind, seed), digest in KNOWN.items():
    data = run(kind, "--count", "1000", "--seed", seed, "--threads", "4").stdout
    got = hashlib.sha256(data).hexdigest()
    check(got == digest, f"{kind} --seed {seed}: sha256 {got} != {digest}")

if failures:
    print("FAIL")
    for failure in failures:
        print(" -", failure)
    sys.exit(1)
print("PASS: dasmig-gen usage errors, 12 kinds, thread invariance, ranges, formats, requests and known outputs.")
