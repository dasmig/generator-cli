#!/usr/bin/env python3
"""Tests for dasmig-gen: python3 tests/test_cli.py build/dasmig-gen

The known-output hashes are the SHA-256 of the exact bytes of a run. They are
the same on every platform: that is the point of the libraries' own random
engine, and the CI checks it on Linux (x86-64 and ARM64), macOS and Windows.
"""

import collections
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
         "occupation", "user", "family", "device", "weather", "locale", "timezone", "identity", "finance", "property", "company"]

KNOWN = {
    ("persona", "1f"): "fb66afc902755597430c9a56afaf0635a4bc1f912b21c2ff4ca5cb4ca65483d7",
    ("name", "2a"): "92523b6a46b7abfb0df42cedcac244cfccb4c5e358e8cd9dd4fb35e28f2987c9",
    ("city", "3"): "18363ff56b0229a14f7544258d3fc26e17c95d398a18683e605da11340f86aef",
    ("occupation", "4"): "7f63ac27e08c59e4fe327433adb30b15e6d45ca54f6fb3ba0ab7128dd32b3d00",
    ("family", "6"): "086654befa2efa30e64e440909bcb3a04bc914f10ac2bd7806f85200b9e79588",
    ("user", "5"): "2b7d085436fe922b24ecfe714160f1330b9a38522e89c284d2f7c4348eecd8ab",
    ("device", "9"): "503a080797d53c0db050c6497424f68cee1fa3b6019cf1d9278f671ec175678f",
    ("weather", "b"): "b02b26c0d3fb045f51d23a880c39d192479e4e5576451b639a3e28fbbc255f35",
    ("locale", "d"): "2e92d7800b67828229a95af5d214a307aeaa11a3c8eebc924d992adbafa6b9bb",
    ("timezone", "e"): "5d2f107f9f577ef099e00cff37909306a9db57a2701bb2ca085f3e1134673ac1",
    ("identity", "a1"): "1ffda2faf568fa128a48e7ad1f5ad6e924c530f8a458c57bd793b7fefaad5809",
    ("finance", "a2"): "fed7ad8b3e1b18763e72073d5a89f0967aafdcf645652e89bececf2cfe4cf7ca",
    ("property", "a3"): "ce25fe7eb2bfad8fba7e9bfba5a3ed79ebc181969bda227be0bece92ea924c80",
    ("company", "a4"): "a49b9da0979afac8b12850a6fab8ffc617c67cf969c4782666af6476d87dc6f2",
}
# company --finance --safe --by-employment --seed a5, and persona --employer --seed 9.
KNOWN_COMPANY_FULL = "ca69fbab3135062bab14ec1677a269a85edccbc8f21f77093929b3713cc6dd22"
KNOWN_PERSONA_EMPLOYER = "cb1a47cc1ae557e9470d9af21a7e5fbd024af6aed589a41354bd5487f97792d6"
# weather --lat 35.68 --lon 139.69 --start 2024-03-01 --days 60 --hourly --units imperial --seed c, 20 items.
KNOWN_WEATHER_SERIES = "454fd34ed013d11f3242605a3faca61b925ee9536afec9a5d379c32427aeed3d"
# user --device --seed 5: the user known output above, with each user's device.
# holidays --country BR --year 2025 --optional --observances, and US 2022 (an observed day in 2021).
KNOWN_HOLIDAYS_BR = "b0761472942c1ea2bf588ed8949cfd1ba92295703ef8ee14c8085399185d3a98"
KNOWN_HOLIDAYS_US = "3aba1f5814e3b5e8b02eeafe7c1f925fd37a192c7b156506c610adfa0d09f854"
KNOWN_USER_DEVICE = "37680a417820994c14546339ea7e6124a879bb3a8b26581047f4fe29275c2f7d"

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
             ["persona", "--lat", "1"],
             ["holidays"], ["holidays", "--country", "BR"], ["holidays", "--year", "2025"],
             ["holidays", "--country", "BR", "--year", "2025", "--seed", "1"],
             ["holidays", "--country", "BR", "--year", "2025", "--count", "2"],
             ["holidays", "--country", "BR", "--year", "2025", "--threads", "2"],
             ["holidays", "--country", "BR", "--year", "2025", "--first", "1"],
             ["holidays", "--country", "BR", "--year", "2025", "--uniform"],
             ["holidays", "--country", "BR", "--year", "x"], ["locale", "--year", "2025"],
             ["locale", "--optional"], ["timezone", "--language", "fr"], ["timezone", "--observances"],
             ["persona", "--language", "fr"]]:
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
run("holidays", "--country", "BR", "--year", "1960", expect=1)  # before the span
run("holidays", "--country", "XX", "--year", "2025", expect=1)
run("locale", "--language", "xx", "--seed", "1", expect=1)
run("locale", "--country", "XX", "--seed", "1", expect=1)
run("timezone", "--reference-date", "1990-01-01", "--seed", "1", expect=1)  # lite's rules start in 2000
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
for kind in ("persona", "name", "city", "family", "device", "weather", "locale", "timezone"):
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
# A job and an income are null for some records. The columns are there even
# when the first record has neither (occupation seed 2 starts inactive with no
# income, persona seed 1 with a student), and later records fill them.
for kind, seed, prefix in [("occupation", "2", ""), ("persona", "1", "occupation.")]:
    out = run(kind, "--count", "40", "--seed", seed, "--format", "csv").stdout.decode("utf-8")
    records = list(csv.DictReader(io.StringIO(out, newline="")))
    items = [json.loads(r) for r in lines(run(kind, "--count", "40", "--seed", seed).stdout)]
    occupations = [item["occupation"] if prefix else item for item in items]
    check(occupations[0]["income"] is None, f"{kind} csv: the first record should have no income")
    check({prefix + c for c in ("job.key", "job.label", "income.amount", "income.currency")} <= set(records[0]),
          f"{kind} csv: job and income columns")
    check(prefix + "income" not in records[0] and prefix + "job" not in records[0], f"{kind} csv: null object column")
    check([r[prefix + "income.currency"] for r in records] == [o["income"]["currency"] if o["income"] else ""
          for o in occupations], f"{kind} csv: income cells")
    check(any(o["income"] for o in occupations), f"{kind} csv: some record should have an income")
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

# --- locale, timezone and holidays ---
for row in lines(run("locale", "--count", "50", "--seed", "2", "--country", "ca").stdout):
    loc = json.loads(row)
    check(loc["country"] == "CA" and loc["tag"].endswith("-CA"), "locale --country")
french = [json.loads(r) for r in lines(run("locale", "--count", "50", "--seed", "2", "--language", "FR").stdout)]
check(all(loc["language"] == "fr" for loc in french) and len({loc["country"] for loc in french}) > 3,
      "locale --language: French tags in several countries")
# India's 14 tags: by speakers, hi-IN is about a third of the draws; uniformly, no tag is near that.
top = lambda *extra: max(collections.Counter(json.loads(r)["tag"] for r in lines(
    run("locale", "--count", "200", "--seed", "2", "--country", "IN", *extra).stdout)).values())
check(top("--uniform") * 2 < top(), "locale --uniform spreads the draws over the tags")
# A locale, a zone and a country drawn with one seed are in the same country.
same = zip(lines(run("locale", "--count", "40", "--seed", "8").stdout),
           lines(run("timezone", "--count", "40", "--seed", "8").stdout),
           lines(run("country", "--count", "40", "--seed", "8").stdout))
check(all(json.loads(a)["country"] == json.loads(b)["country"] == json.loads(c)["cca2"] for a, b, c in same),
      "locale, timezone and country agree on the country")
summer = json.loads(run("timezone", "--country", "DE", "--seed", "1").stdout)
winter = json.loads(run("timezone", "--country", "DE", "--seed", "1", "--reference-date", "2024-01-15").stdout)
check(summer["name"] == winter["name"] == "Europe/Berlin" and summer["abbreviation"] == "CEST"
      and winter["abbreviation"] == "CET" and winter["utc_offset"] == "+01:00", "timezone --reference-date")
br = [json.loads(r) for r in lines(run("holidays", "--country", "br", "--year", "2025").stdout)]
check(len(br) >= 9 and br[0]["date"] == "2025-01-01" and br[-1]["key"] == "christmas_day"
      and all(h["kind"] == "public" for h in br) and [h["date"] for h in br] == sorted(h["date"] for h in br),
      "holidays: Brazil 2025, public days by date")
more = [json.loads(r) for r in lines(run("holidays", "--country", "BR", "--year", "2025", "--optional").stdout)]
check(any(h["key"] == "carnival" and h["kind"] == "optional" for h in more) and len(more) > len(br),
      "holidays --optional adds Carnival")
check(not any(h["kind"] == "observance" for h in more), "no observances unless asked")
us = [json.loads(r) for r in lines(run("holidays", "--country", "US", "--year", "2022").stdout)]
check(us[0]["key"] == "new_year_s_day" and us[0]["observed"] == "2021-12-31", "holidays: an observed day")
check(run("holidays", "--country", "BR", "--year", "2025").stderr == b"", "holidays prints no seed")
table = list(csv.reader(io.StringIO(run("holidays", "--country", "BR", "--year", "2025", "--format", "csv")
                                   .stdout.decode("utf-8"), newline="")))
check(table[0][:3] == ["date", "observed", "key"] and len(table) == len(br) + 1, "holidays csv")
check(json.loads(run("holidays", "--country", "BR", "--year", "2025", "--format", "json").stdout) == br,
      "holidays json equals jsonl")

# --- known outputs, the same on every platform ---
for (kind, seed), digest in KNOWN.items():
    data = run(kind, "--count", "1000", "--seed", seed, "--threads", "4").stdout
    got = hashlib.sha256(data).hexdigest()
    check(got == digest, f"{kind} --seed {seed}: sha256 {got} != {digest}")
got = hashlib.sha256(run("company", "--finance", "--safe", "--by-employment", "--count", "1000", "--seed", "a5",
                         "--threads", "4").stdout).hexdigest()
check(got == KNOWN_COMPANY_FULL, f"company --finance --safe --by-employment --seed a5: sha256 {got} != {KNOWN_COMPANY_FULL}")
got = hashlib.sha256(run("persona", "--employer", "--count", "1000", "--seed", "9", "--threads", "4").stdout).hexdigest()
check(got == KNOWN_PERSONA_EMPLOYER, f"persona --employer --seed 9: sha256 {got} != {KNOWN_PERSONA_EMPLOYER}")
got = hashlib.sha256(run("user", "--device", "--count", "1000", "--seed", "5", "--threads", "4").stdout).hexdigest()
check(got == KNOWN_USER_DEVICE, f"user --device --seed 5: sha256 {got} != {KNOWN_USER_DEVICE}")
got = hashlib.sha256(run("weather", "--lat", "35.68", "--lon", "139.69", "--start", "2024-03-01", "--days", "60",
                         "--hourly", "--units", "imperial", "--count", "20", "--seed", "c", "--threads", "4").stdout).hexdigest()
check(got == KNOWN_WEATHER_SERIES, f"weather series --seed c: sha256 {got} != {KNOWN_WEATHER_SERIES}")
got = hashlib.sha256(run("holidays", "--country", "BR", "--year", "2025", "--optional", "--observances").stdout).hexdigest()
check(got == KNOWN_HOLIDAYS_BR, f"holidays BR 2025: sha256 {got} != {KNOWN_HOLIDAYS_BR}")
got = hashlib.sha256(run("holidays", "--country", "US", "--year", "2022").stdout).hexdigest()
check(got == KNOWN_HOLIDAYS_US, f"holidays US 2022: sha256 {got} != {KNOWN_HOLIDAYS_US}")

# --- a name from the country asked for ---
for row in lines(run("name", "--country", "br", "--count", "30", "--seed", "1").stdout):
    check(json.loads(row)["culture"] == "brazilian", "name --country")
for row in lines(run("surname", "--country", "JP", "--count", "30", "--seed", "1").stdout):
    check(json.loads(row)["culture"] == "japanese", "surname --country")
run("name", "--country", "XX", "--seed", "1", expect=1)

# --- identity and finance ---
def items(*args):
    return [json.loads(r) for r in lines(run(*args).stdout)]


for args in (["identity", "--document", "licence"], ["identity", "--unique", "email"], ["identity", "--age", "30"],
             ["identity", "--accounts", "current"], ["finance", "--document", "passport"],
             ["finance", "--accounts", "pension"], ["finance", "--cards", "gift"], ["finance", "--card-source", "real"],
             ["finance", "--unique", "number"], ["finance", "--age", "30"], ["finance", "--sex", "male"],
             ["user", "--document", "passport"], ["user", "--cards", "any"], ["persona", "--scheme", "br-cpf"],
             ["persona", "--network", "visa"], ["persona", "--safe"], ["family", "--identity"], ["device", "--finance"],
             ["validate"], ["validate", "br-cpf"], ["validate", "br-cpf", "1", "2"], ["validate", "br-cpf", "1", "--seed", "1"],
             ["validate", "br-cpf", "1", "--count", "2"], ["validate", "br-cpf", "1", "--safe"],
             ["validate", "iban-de", "1", "--reference-date", "2020-01-01"], ["validate", "card", "1", "--reference-date", "2020-01-01"],
             ["name", "x"]):
    run(*args, expect=2)
run("identity", "--country", "XX", "--seed", "1", expect=1)
run("identity", "--scheme", "no-such", "--country", "BR", "--seed", "1", expect=1)
run("finance", "--country", "XX", "--seed", "1", expect=1)
run("finance", "--safe", "--cards", "any", "--card-source", "network_range", "--seed", "1", expect=1)
run("identity", "--with-name", "--country", "AQ", "--seed", "1", expect=1)
run("validate", "no-such", "1", expect=1)

people = items("identity", "--country", "br", "--count", "30", "--seed", "1", "--document", "passport", "--reference-date", "2025-01-01")
check(all(p["country_code"] == "BR" and all(n["scheme"] == "br-cpf" for n in p["numbers"]) for p in people)
      and any(p["numbers"] for p in people), "identity --country")
check(all(p["documents"][0]["mrz"] == [] for p in people if p["documents"]) and "name" not in people[0], "identity without a name has no MRZ")
named = items("identity", "--with-name", "--country", "BR", "--count", "30", "--seed", "1", "--document", "passport", "--sex", "female", "--age", "30-40")
check(all(p["name"] and p["sex"] == "female" and p["documents"][0]["mrz"] for p in named if p["documents"]), "identity --with-name")
check(any(p["name"].isascii() and p["name"].upper().split()[-1] in p["documents"][0]["mrz"][0] for p in named if p["documents"]),
      "identity --with-name: the name is on the document")
check(any(p["documents"] for p in named), "identity --with-name: documents")
drawn = items("identity", "--with-name", "--count", "200", "--seed", "2")
check(len({p["country_code"] for p in drawn}) > 20 and all(p["name"] for p in drawn), "identity --with-name draws countries with names")
safe = items("identity", "--safe", "--country", "FR", "--count", "20", "--seed", "3", "--document", "passport")
check(all(n["reserved"] for p in safe for n in p["numbers"]), "identity --safe")
batch = items("identity", "--country", "BR", "--count", "3000", "--seed", "4", "--unique", "number", "--threads", "3")
values = [n["value"] for p in batch for n in p["numbers"][:1]]
check(len(batch) == 3000 and len(values) > 2000 and len(set(values)) == len(values), "identity --unique number")
# Countries that often have no number still fill a unique batch.
check(len(items("identity", "--count", "500", "--seed", "4", "--unique", "number")) == 500, "identity --unique: no number is no duplicate")
for a in (["identity"], ["finance", "--cards", "any"], ["identity", "--with-name"], ["finance", "--with-name"]):
    one = run(*a, "--count", "50", "--seed", "6", "--threads", "1").stdout
    check(one == run(*a, "--count", "50", "--seed", "6", "--threads", "4").stdout, f"{a}: threads")
    check(lines(one)[7:9] == lines(run(*a, "--count", "2", "--first", "7", "--seed", "6").stdout), f"{a}: --first")

money = items("finance", "--country", "DE", "--count", "20", "--seed", "1", "--accounts", "current,savings",
              "--cards", "debit,any", "--card-source", "test_number")
check(all(len(m["accounts"]) == 2 and m["accounts"][0]["iban"].startswith("DE") and len(m["cards"]) == 2
          and all(c["source"] == "test_number" and c["reserved"] for c in m["cards"]) for m in money), "finance request")
check(all(c["holder"] == "" for m in money for c in m["cards"]), "finance without a name has no holder")
held = items("finance", "--with-name", "--country", "FR", "--count", "20", "--seed", "1", "--cards", "any", "--card-source", "test_number")
check(all(m["name"] and m["cards"] and m["cards"][0]["holder"] for m in held), "finance --with-name")
check(items("finance", "--country", "DE", "--accounts", "none", "--seed", "1")[0]["accounts"] == [], "finance --accounts none")
check(all(c["network"] == "visa" for m in items("finance", "--country", "US", "--count", "10", "--seed", "1", "--cards", "debit", "--network", "visa") for c in m["cards"]), "finance --network")
range_cards = items("finance", "--country", "US", "--count", "10", "--seed", "1", "--cards", "debit")
check(all(c["source"] == "network_range" and c["security_code"] == "" for m in range_cards for c in m["cards"]), "finance: range cards by default, no security code")
safe = items("finance", "--country", "FR", "--count", "10", "--seed", "1", "--safe", "--cards", "any")
check(all(a["reserved"] for m in safe for a in m["accounts"]) and all(c["reserved"] for m in safe for c in m["cards"]), "finance --safe")
batch = items("finance", "--country", "DE", "--count", "2000", "--seed", "4", "--unique", "account")
check(len({m["accounts"][0]["iban"] for m in batch}) == 2000, "finance --unique account")
batch = items("finance", "--country", "US", "--count", "2000", "--seed", "4", "--unique", "card", "--cards", "debit")
check(len({m["cards"][0]["number"] for m in batch}) == 2000, "finance --unique card")
table = list(csv.reader(io.StringIO(run("identity", "--country", "BR", "--count", "3", "--seed", "1", "--with-name", "--format", "csv").stdout.decode("utf-8"))))
check(table[0][:2] == ["name", "country_code"] and len(table) == 4, "identity csv")

# --- user and persona with the parts: nothing else changes ---
def bare(item):
    return {k: v for k, v in item.items() if k not in ("identity", "finance", "signature")}


for kind in ("user", "persona"):
    plain = items(kind, "--count", "30", "--seed", "8")
    full = items(kind, "--count", "30", "--seed", "8", "--identity", "--document", "passport", "--finance", "--cards", "any", "--card-source", "test_number")
    check(all("identity" not in p and "finance" not in p for p in plain), f"{kind}: no parts by default")
    check([bare(p) for p in plain] == [bare(p) for p in full], f"{kind}: the parts change nothing else")
    check(all(isinstance(p["identity"]["numbers"], list) and isinstance(p["finance"]["accounts"], list) for p in full), f"{kind}: the parts")
    check(all(p["finance"]["cards"][0]["holder"] for p in full if p["finance"]["cards"]), f"{kind}: card holders")
user = items("user", "--country", "BR", "--seed", "2a", "--identity", "--safe")[0]
check(user["identity"]["numbers"][0]["reserved"], "user --safe reaches the papers")
user = items("persona", "--country", "BR", "--seed", "2a", "--identity", "--safe")[0]
check(user["identity"]["numbers"][0]["reserved"], "persona --safe reaches the papers")
table = list(csv.reader(io.StringIO(run("user", "--count", "2", "--seed", "1", "--identity", "--finance", "--format", "csv").stdout.decode("utf-8"))))
check("identity.numbers" in table[0] and "finance.accounts" in table[0], "user csv with the parts")

# --- property, and persona --property ---
home = items("property", "--country", "BR", "--count", "30", "--seed", "1", "--income", "90000", "--household-size", "4",
             "--household-type", "nuclear", "--children", "2", "--age", "40", "--type", "detached", "--tenure", "owner_mortgage",
             "--area", "town", "--reference-date", "2020-01-01")
check(all(h["country_code"] == "BR" and h["household_size"] == 4 and h["household_type"] == "nuclear" and h["type"] == "detached"
          and h["holding"] == "owner_mortgage" and h["area"] == "town" and h["year_built"] <= 2020
          and h["monthly_cost"]["currency"] == "BRL" for h in home), "property request")
check(all(h["monthly_cost"] is None for h in items("property", "--country", "DE", "--count", "10", "--seed", "1")),
      "property: no cost without an income")
table = list(csv.reader(io.StringIO(run("property", "--count", "5", "--seed", "1", "--format", "csv").stdout.decode("utf-8"))))
check(table[0][:2] == ["country_code", "area"] and "monthly_cost.amount" in table[0] and len(table) == 6, "property csv")
for args in (["property", "--income", "5"], ["property", "--type", "castle"], ["property", "--tenure", "squatter"],
             ["property", "--area", "suburb"], ["property", "--household-type", "commune"], ["property", "--age", "12"],
             ["property", "--age", "30-40"], ["property", "--household-size", "0"], ["property", "--sex", "male"],
             ["persona", "--type", "detached"], ["persona", "--area", "city"], ["property", "--property"]):
    run(*args, "--seed", "1", expect=2)
run("property", "--country", "XX", "--seed", "1", expect=1)
run("property", "--household-type", "one_person", "--household-size", "3", "--seed", "1", expect=1)
plain = items("persona", "--count", "30", "--seed", "9")
housed = items("persona", "--count", "30", "--seed", "9", "--property")
check(all("home" not in p for p in plain), "persona: no home by default")
check([{k: v for k, v in p.items() if k not in ("home", "household_income", "signature")} for p in housed]
      == [{k: v for k, v in p.items() if k != "signature"} for p in plain], "persona: a home changes nothing else")
check(all(p["home"]["country_code"] == p["country"]["cca2"] for p in housed), "persona: the home is in the country")
family = items("persona", "--count", "20", "--seed", "9", "--property", "--relations", "--type", "flat_large")
check(all("household" in p["relatives"] and p["home"]["type"] == "flat_large" for p in family),
      "persona --property --relations: the family's household, and --type")
table = list(csv.reader(io.StringIO(run("persona", "--count", "3", "--seed", "1", "--property", "--format", "csv").stdout.decode("utf-8"))))
check("home.type" in table[0] and "home.monthly_cost.amount" in table[0] and "household_income.amount" in table[0],
      "persona csv with a home")

# --- company, and persona --employer ---
firms = items("company", "--country", "DE", "--count", "60", "--seed", "1", "--isic", "F", "--size", "small",
              "--incorporated-only", "--legal-form", "GmbH", "--safe", "--finance")
check(all(f["country_code"] == "DE" and f["isic_section"] == "F" and f["size"] == "small" and f["legal_form"] == "GmbH"
          and f["domain"].endswith(".example") and all(n["reserved"] for n in f["numbers"])
          and f["address"]["country_code"] == "DE" and f["phone"]["reserved"] and f["phone"]["country_code"] == "DE"
          and f["email"]["address"] == f["email"]["local"] + "@" + f["domain"]
          and f["finance"]["accounts"] and "birth_date" not in f["finance"] for f in firms), "company request")
check(all(10 <= f["employees"] <= 49 for f in items("company", "--count", "40", "--seed", "2", "--size", "small")), "company --size")
check(all(f["employees"] == 7 for f in items("company", "--count", "20", "--seed", "2", "--employees", "7")), "company --employees")
bare = items("company", "--count", "20", "--seed", "3")
check(all("finance" not in f and "address" in f and "phone" in f and "email" in f for f in bare), "company: parts, finance only when asked")
check(sum(f["employees"] >= 50 for f in items("company", "--count", "300", "--seed", "4", "--by-employment"))
      > sum(f["employees"] >= 50 for f in items("company", "--count", "300", "--seed", "4")), "company --by-employment")
# Countries phone-generator has no numbers for get a null phone, and the batch goes on.
check(all(f["phone"] is None for f in items("company", "--country", "TW", "--count", "5", "--seed", "1")), "company: no phone numbers in TW")
check(all(f["address"] and f["phone"] for f in items("company", "--country", "JP", "--count", "5", "--seed", "1")), "company JP")
table = list(csv.reader(io.StringIO(run("company", "--count", "400", "--seed", "7", "--format", "csv", "--finance").stdout.decode("utf-8"))))
check(len(table) == 401 and all(len(row) == len(table[0]) for row in table), "company csv: one row each, no ragged rows")
check("name" in table[0] and "address.city" in table[0] and "phone.e164" in table[0] and "email.address" in table[0]
      and "finance.accounts" in table[0], "company csv columns")
check("phone" not in table[0], "company csv: a null phone is written as its columns")
check(run("company", "--count", "300", "--seed", "1f", "--threads", "1", "--finance").stdout
      == run("company", "--count", "300", "--seed", "1f", "--threads", "7", "--finance").stdout, "company: threads agree")
check(run("company", "--count", "10", "--seed", "1f", "--first", "5").stdout
      == b"".join(r.encode("utf-8") + b"\n" for r in lines(run("company", "--count", "15", "--seed", "1f").stdout)[5:]),
      "company: --first")
for args in (["company", "--sex", "male"], ["company", "--size", "huge"], ["company", "--size", "small", "--employees", "7"],
             ["company", "--employees", "0"], ["company", "--employees", "x"], ["company", "--unique", "email"],
             ["company", "--accounts", "current"], ["persona", "--isic", "F"], ["persona", "--size", "small"],
             ["persona", "--incorporated-only"], ["persona", "--legal-form", "GmbH"], ["persona", "--employees", "3"],
             ["name", "--employer"], ["user", "--finance", "--employer"], ["company", "--employer"],
             ["company", "--by-employment", "--employees", "-1"]):
    run(*args, "--seed", "1", expect=2)
run("company", "--country", "XX", "--seed", "1", expect=1)
run("company", "--isic", "99", "--seed", "1", expect=1)
run("company", "--country", "DE", "--legal-form", "LLC", "--seed", "1", expect=1)
plain = items("persona", "--count", "60", "--seed", "9")
hired = items("persona", "--count", "60", "--seed", "9", "--employer")
check(all("employer" not in p for p in plain), "persona: no employer by default")
check([{k: v for k, v in p.items() if k not in ("employer", "signature")} for p in hired]
      == [{k: v for k, v in p.items() if k != "signature"} for p in plain], "persona: an employer changes nothing else")
check(all((p["employer"] is None) == (p["occupation"]["status"] != "employed") for p in hired), "persona: only the employed have an employer")
check(all(p["employer"]["country_code"] == p["country"]["cca2"] for p in hired if p["employer"]), "persona: the employer is in the country")
check(any(p["employer"] is None for p in hired) and any(p["employer"] for p in hired), "persona: both employed and not")
small = items("persona", "--count", "60", "--seed", "9", "--employer", "--size", "micro", "--safe", "--country", "ES")
check(all(p["employer"]["size"] == "micro" and p["employer"]["domain"].endswith(".example") for p in small if p["employer"]),
      "persona --employer passes --size and --safe through")
table = list(csv.reader(io.StringIO(run("persona", "--count", "60", "--seed", "9", "--employer", "--format", "csv").stdout.decode("utf-8"))))
check(len(table) == 61 and all(len(row) == len(table[0]) for row in table), "persona --employer csv: no ragged rows")
check("employer.name" in table[0] and "employer" not in table[0], "persona --employer csv: columns for a null employer")

# --- validate ---
def validate(*args, expect=0):
    done = run("validate", *args, expect=expect)
    check(done.stderr == b"", "validate prints no seed")
    return json.loads(done.stdout)


check(validate("br-cpf", "111.444.777-35") == {"scheme": "br-cpf", "text": "111.444.777-35", "valid": True}, "validate: a CPF")
check(not validate("br-cpf", "111.444.777-36", expect=3)["valid"], "validate: a wrong CPF")
check(validate("iban-de", "DE89 3704 0044 0532 0130 00")["valid"], "validate: an IBAN")
check(not validate("iban-de", "DE89 3704 0044 0532 0130 01", expect=3)["valid"], "validate: a wrong IBAN")
check("visa" in validate("card", "4242424242424242")["networks"], "validate: a card and its networks")
check(validate("card", "4242424242424241", expect=3)["networks"] == [], "validate: a wrong card")
for source in items("identity", "--country", "IT", "--count", "20", "--seed", "1", "--with-name"):
    for number in source["numbers"]:
        validate(number["scheme"], number["value"])
for source in items("finance", "--country", "BR", "--count", "20", "--seed", "1"):
    for account in source["accounts"]:
        validate(account["scheme"], account["iban"])
check(lines(run("validate", "card", "4242424242424242", "--format", "csv").stdout)[0].startswith("scheme,text,valid"), "validate csv")

if failures:
    print("FAIL")
    for failure in failures:
        print(" -", failure)
    sys.exit(1)
print("PASS: dasmig-gen usage errors, 20 kinds, holidays and validate, thread invariance, ranges, formats, requests and known outputs.")
