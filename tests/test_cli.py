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
         "occupation", "user", "family", "device", "weather"]

KNOWN = {
    ("persona", "1f"): "4f5aaf6a50517742898a15a1a71da8a827fe86066d7a77b5a3488e555b35db8a",
    ("name", "2a"): "92523b6a46b7abfb0df42cedcac244cfccb4c5e358e8cd9dd4fb35e28f2987c9",
    ("city", "3"): "18363ff56b0229a14f7544258d3fc26e17c95d398a18683e605da11340f86aef",
    ("occupation", "4"): "a3a8e38268b29c82c38bda278d69ac1930ecabc236f5dbd61ddd0fb0f2c1a605",
    ("family", "6"): "086654befa2efa30e64e440909bcb3a04bc914f10ac2bd7806f85200b9e79588",
    ("user", "5"): "ced64388c6f120352a8c599858a978ecc852a1b92bdd676e3cb087e588b523b9",
    ("device", "9"): "503a080797d53c0db050c6497424f68cee1fa3b6019cf1d9278f671ec175678f",
    ("weather", "b"): "b02b26c0d3fb045f51d23a880c39d192479e4e5576451b639a3e28fbbc255f35",
}
# weather --lat 35.68 --lon 139.69 --start 2024-03-01 --days 60 --hourly --units imperial --seed c, 20 items.
KNOWN_WEATHER_SERIES = "454fd34ed013d11f3242605a3faca61b925ee9536afec9a5d379c32427aeed3d"
# user --device --seed 5: the user known output above, with each user's device.
KNOWN_USER_DEVICE = "021aa999ba6263a54d5399b94ef496b3c0e3ed1fd3e1c833519c078550030015"

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
             ["user", "--unique", "city"], ["city", "--unique", "email"], ["persona", "--safe"],
             ["family", "--traits", "3"], ["family", "--up", "x"], ["family", "--age", "20-30"],
             ["persona", "--up", "1"], ["persona", "--home"], ["persona", "--stepparents"], ["name", "--home"], ["persona", "--down", "1"], ["name", "--relations"], ["family", "--relations"],
             ["device", "--ip", "v5"], ["device", "--unique", "email"], ["user", "--unique", "ip"],
             ["device", "--sex", "male"], ["device", "--age", "30"], ["name", "--platform", "ipad"],
             ["persona", "--device"], ["device", "--device"], ["user", "--browser", "chrome"],
             ["weather", "--lat", "10"], ["weather", "--lon", "10"], ["weather", "--lat", "x", "--lon", "1"],
             ["weather", "--city", "--lat", "1", "--lon", "1"], ["weather", "--country", "FR"],
             ["weather", "--units", "kelvin"], ["weather", "--days", "0"], ["weather", "--days", "36601"],
             ["weather", "--start", "2023-13-01"], ["weather", "--sex", "male"], ["city", "--hourly"],
             ["persona", "--lat", "1"]]:
    result = run(*args, expect=2)
    check(b"dasmig-gen:" in result.stderr, f"usage error message for {args}")
run("persona", "--country", "XX", "--count", "1", "--seed", "1", expect=1)
run("user", "--age", "5-10", "--count", "1", "--seed", "1", expect=1)
run("device", "--platform", "nope", "--count", "1", "--seed", "1", expect=1)
run("device", "--platform", "ipad", "--browser", "samsung", "--count", "1", "--seed", "1", expect=1)
run("device", "--reference-date", "2020-12-31", "--count", "1", "--seed", "1", expect=1)
run("device", "--locale", "not a tag", "--count", "1", "--seed", "1", expect=1)
run("weather", "--lat", "40", "--lon", "-40", "--seed", "1", expect=1)  # mid-Atlantic
run("weather", "--lat", "48.85", "--lon", "2.35", "--warming", "11", "--seed", "1", expect=1)
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
for kind in ("persona", "name", "city", "family", "device", "weather"):
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
# --relations adds the family as "relatives" and changes nothing else; --up and --down shape it
plain = [json.loads(r) for r in lines(run("persona", "--count", "30", "--seed", "1f").stdout)]
with_family = [json.loads(r) for r in lines(run("persona", "--count", "30", "--seed", "1f", "--relations").stdout)]
check(all("relatives" not in p for p in plain), "persona has no relatives by default")
check(all("relatives" in p for p in with_family), "persona --relations has relatives")
check([{k: v for k, v in p.items() if k != "relatives"} for p in with_family] == plain,
      "--relations changes nothing but relatives")
check(run("persona", "--count", "30", "--seed", "1f", "--relations", "--threads", "0").stdout
      == run("persona", "--count", "30", "--seed", "1f", "--relations").stdout, "relations: threads")
shallow = [json.loads(r) for r in lines(run("persona", "--count", "30", "--seed", "1f", "--relations",
                                            "--up", "0", "--down", "0").stdout)]
check(all({m["role"] for m in p["relatives"]["members"]} <= {"anchor", "partner"} for p in shallow),
      "relations --up 0 --down 0 leaves no parents, siblings or children")
check(all(any(m["role"] == "anchor" and m["name"]["full"] == p["name"] and m["age"] == p["age"]
              for m in p["relatives"]["members"]) for p in with_family),
      "the anchor of the relatives is the persona")
# --stepparents and --home are off by default and add to the family
homed = [json.loads(r) for r in lines(run("persona", "--count", "300", "--seed", "1f", "--relations",
                                          "--stepparents", "--home").stdout)]
check(all("household" in p["relatives"] and p["relatives"]["household"]["members"][0] == 0 for p in homed),
      "persona --home has a household")
check(all("household" not in p["relatives"] for p in with_family), "no household by default")
check(any(m["role"] in ("stepfather", "stepmother") for p in homed for m in p["relatives"]["members"]),
      "persona --stepparents has stepparents")
fam_plain = lines(run("family", "--count", "20", "--seed", "3").stdout)
fam_home = [json.loads(r) for r in lines(run("family", "--count", "20", "--seed", "3", "--home").stdout)]
check(all("household" in f for f in fam_home) and all("household" not in json.loads(r) for r in fam_plain),
      "family --home")
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

# --- device: the request, safe addresses and unique keys; user --device ---
for row in lines(run("device", "--count", "30", "--seed", "2", "--country", "br", "--platform", "ios-phone",
                     "--browser", "safari", "--locale", "pt-BR", "--ip", "v4", "--reference-date", "2025-03-01").stdout):
    d = json.loads(row)
    check(d["country_code"] == "BR" and d["platform"] == "ios-phone" and d["os"] == "iOS" and d["browser"] == "safari"
          and d["ip_version"] == 4 and d["accept_language"].startswith("pt-BR") and "iPhone" in d["user_agent"],
          "device request fields")
check(all(json.loads(r)["ip_version"] == 6 for r in lines(run("device", "--count", "30", "--seed", "2", "--country",
                                                             "US", "--ip", "v6").stdout)), "device --ip v6")
safe = [json.loads(r) for r in lines(run("device", "--count", "50", "--seed", "4", "--safe").stdout)]
check(len(safe) == 50 and all(d["reserved"] for d in safe), "device --safe")
check(not any(json.loads(r)["reserved"] for r in lines(run("device", "--count", "50", "--seed", "4").stdout)),
      "device addresses are not reserved by default")
uniform = [json.loads(r) for r in lines(run("device", "--count", "300", "--seed", "4", "--uniform").stdout)]
check(len(uniform) == 300 and len({d["platform"] for d in uniform}) >= 8, "device --uniform reaches every platform")
batch = [json.loads(r) for r in lines(run("device", "--count", "2000", "--seed", "7", "--country", "IS",
                                          "--unique", "ip", "--threads", "3").stdout)]
check(len(batch) == 2000 and len({d["ip"] for d in batch}) == 2000, "device unique ip")
plain = [json.loads(r) for r in lines(run("user", "--count", "40", "--seed", "4", "--safe").stdout)]
with_device = [json.loads(r) for r in lines(run("user", "--count", "40", "--seed", "4", "--safe", "--device").stdout)]
check(all("device" not in u for u in plain), "user has no device by default")
check(all(u["device"]["platform"] == u["account"]["platform"] and u["device"]["country_code"] == u["country"]["code"]
          and u["device"]["reserved"] for u in with_device), "user --device follows the account; --safe reaches it")
strip = lambda u: {k: v for k, v in u.items() if k not in ("device", "signature")}
check([strip(u) for u in with_device] == [strip(u) for u in plain], "--device changes nothing else in a user")

# --- family: the request and the generations ---
for row in lines(run("family", "--count", "10", "--seed", "3", "--country", "br", "--sex", "female", "--age", "30",
                     "--up", "0", "--down", "0").stdout):
    f = json.loads(row)
    anchor = f["members"][0]
    check(f["country"] == "BR" and anchor["role"] == "anchor" and anchor["sex"] == "female" and anchor["age"] == 30
          and not any(m["role"] in ("father", "mother", "sibling", "child") for m in f["members"]),
          "family request fields and generations")
for row in lines(run("family", "--count", "5", "--seed", "3", "--country", "JP", "--reference-date", "2030-03-09").stdout):
    check(json.loads(row)["reference_date"] == "2030-03-09", "family reference date")
check(any(m["role"] == "child" for row in lines(run("family", "--count", "50", "--seed", "3", "--down", "1").stdout)
          for m in json.loads(row)["members"]), "family children")

# --- weather: series, units, hours, cities and history ---
paris = ["--lat", "48.85", "--lon", "2.35", "--start", "2024-01-15"]
week = [json.loads(r) for r in lines(run("weather", *paris, "--days", "7", "--count", "2", "--seed", "9").stdout)]
check(len(week) == 14 and [d["date"] for d in week[:7]] == [f"2024-01-{d}" for d in range(15, 22)],
      "weather: --days gives each item's days in order")
check(week[0]["signature"]["seed"] != week[7]["signature"]["seed"], "weather: items have their own seeds")
# A day of a series is the single day with the item's seed (the series does not change a day).
later = json.loads(run("weather", "--lat", "48.85", "--lon", "2.35", "--start", "2024-01-18", "--seed", "9").stdout)
check(later == week[3], "weather: a day alone equals the day in its series")
metric = json.loads(run("weather", *paris, "--seed", "4").stdout)
imperial = json.loads(run("weather", *paris, "--seed", "4", "--units", "imperial").stdout)
check(imperial["units"] == "imperial" and metric["units"] == "metric"
      and abs(imperial["tmax"] - (metric["tmax"] * 1.8 + 32)) < 0.2, "weather: --units imperial")
hourly = json.loads(run("weather", *paris, "--seed", "4", "--hourly").stdout)
check(len(hourly["hours"]) == 24 and {k: v for k, v in hourly.items() if k != "hours"} == metric,
      "weather: --hourly adds the hours and changes nothing else")
check(round(sum(h["precipitation"] for h in hourly["hours"]), 1) == metric["precipitation"], "weather: hours add up")
near = json.loads(run("weather", "--lat", "48.9", "--lon", "2.4", "--start", "2024-01-15", "--seed", "4").stdout)
alone = json.loads(run("weather", "--lat", "48.9", "--lon", "2.4", "--start", "2024-01-15", "--seed", "4",
                       "--no-spatial").stdout)
check(near != alone and near["place"] == alone["place"], "weather: --no-spatial changes the weather, not the place")
check(len(lines(run("weather", *paris, "--seed", "4", "--hourly", "--format", "csv").stdout)) == 2,
      "weather: csv keeps the hours in one column")
for row in lines(run("weather", "--city", "--country", "br", "--count", "20", "--seed", "3").stdout):
    w = json.loads(row)
    check(w["city"]["country_code"] == "BR" and w["place"]["distance_km"] <= 100, "weather: --city --country")
history = next((d for d in (Path(".."), Path("libs"))
                if (d / "weather-generator" / "resources" / "history" / "history.tsv").is_file()), None)
if history is not None:
    jan = [json.loads(r) for r in lines(run("weather", "--lat", "48.85", "--lon", "2.35", "--start", "2010-01-01",
                                            "--days", "31", "--count", "20", "--seed", "1", "--historical",
                                            "--data-dir", str(history)).stdout)]
    normal = [json.loads(r) for r in lines(run("weather", "--lat", "48.85", "--lon", "2.35", "--start", "2010-01-01",
                                               "--days", "31", "--count", "20", "--seed", "1").stdout)]
    mean = lambda days: sum(d["tmean"] for d in days) / len(days)
    check(mean(jan) < mean(normal) - 2, f"weather: January 2010 in Paris was cold ({mean(jan):.1f} vs {mean(normal):.1f})")
    run("weather", "--lat", "48.85", "--lon", "2.35", "--start", "2021-01-01", "--historical",
        "--data-dir", str(history), "--seed", "1", expect=1)
else:
    print("note: no weather-generator checkout with history.tsv next to the CLI; --historical not tested")

# --- known outputs, the same on every platform ---
for (kind, seed), digest in KNOWN.items():
    data = run(kind, "--count", "1000", "--seed", seed, "--threads", "4").stdout
    got = hashlib.sha256(data).hexdigest()
    check(got == digest, f"{kind} --seed {seed}: sha256 {got} != {digest}")
got = hashlib.sha256(run("user", "--device", "--count", "1000", "--seed", "5", "--threads", "4").stdout).hexdigest()
check(got == KNOWN_USER_DEVICE, f"user --device --seed 5: sha256 {got} != {KNOWN_USER_DEVICE}")
got = hashlib.sha256(run("weather", "--lat", "35.68", "--lon", "139.69", "--start", "2024-03-01", "--days", "60",
                         "--hourly", "--units", "imperial", "--count", "20", "--seed", "c", "--threads", "4").stdout).hexdigest()
check(got == KNOWN_WEATHER_SERIES, f"weather series --seed c: sha256 {got} != {KNOWN_WEATHER_SERIES}")

if failures:
    print("FAIL")
    for failure in failures:
        print(" -", failure)
    sys.exit(1)
print("PASS: dasmig-gen usage errors, 15 kinds, thread invariance, ranges, formats, requests and known outputs.")
