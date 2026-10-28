# dasmig-gen

Generate fictional people, places and data in bulk from the command line: JSON Lines, CSV or JSON, with a seed so a run can be repeated exactly, on any machine.

`dasmig-gen` is a single executable over the [dasmig generator libraries](https://github.com/dasmig) (C++23). Their `lite` data is built in, so there is nothing to install or download.

```
dasmig-gen persona --count 100000 --seed 1f --format jsonl --threads 0 --out people.jsonl
dasmig-gen persona --country BR --sex female --age 25-40 --count 1000 --format csv --out brazil.csv
dasmig-gen name --country DE --gender f --count 20
dasmig-gen city --country JP --count 50 --uniform --format json
```

Binaries for Linux (x86-64 and ARM64), macOS (ARM64) and Windows (x86-64) are attached to each [release](https://github.com/dasmig/generator-cli/releases), with `SHA256SUMS`.

## Kinds

| Kind | What it generates | Request options |
| --- | --- | --- |
| `persona` | A complete character: name, country, city, birth, body, personality, interests, job | `--country --sex --age --reference-date --traits --relations --up --down --stepparents --home` |
| `name`, `surname` | A given and family name, or a family name | `--country --gender` |
| `nickname` | A nickname, from `--name` or the word lists | `--name` |
| `city` | A city (GeoNames) | `--country --uniform` |
| `country` | Country facts | `--region --uniform` |
| `birth` | A birth date and age | `--country --sex --age --reference-date --uniform` |
| `biodata` | Height, weight, eyes, hair, skin, blood type | `--country --sex --age` |
| `trait` | A personality | `--traits` |
| `interest` | Religion, hobbies and media | `--country --sex --age` |
| `occupation` | Employment, education and job | `--country --sex --age` |
| `user` | A user record: name, birth, address, phone, email and an account (username, password, dates, device, locale) | `--country --sex --age --reference-date --safe --unique --device` |
| `family` | The people around a person: parents, siblings, partners and children, with ages, deaths and names by country | `--country --sex --age --reference-date --uniform --up --down --stepparents --home` |
| `device` | A device whose model, OS, browser, user agent, client hints, screen and IP address fit together at a date | `--country --reference-date --platform --browser --ip --locale --safe --unique --uniform` |

What a request leaves out is drawn. `--age` is `N` or `MIN-MAX` (completed years, 0-120; `biodata`, `interest`, `occupation` and `family` take one age; `user` ages start at 13). Countries are ISO 3166-1 alpha-2 codes. The generator never reads the clock: pass `--reference-date YYYY-MM-DD` for ages as of a date.

`family --up N --down N` sets how many generations above and below the person to generate (1 each by default; 0 leaves them out), and the person is the one `--country`, `--sex` and `--age` describe.

`persona --relations` also draws the persona's family (parents, siblings, partners and children) into a `relatives` field, with `--up`, `--down`, `--stepparents` and `--home` as for `family` (`--stepparents` adds a parent's new partner, the children the parent had with them and their own children from before as `stepsibling`, `--home` adds who the person lives with as `household`; both are off by default and need `--relations` for a persona). Nothing else about the persona changes, so the same seed gives the same persona with and without it.

`user --safe` makes contacts that reach no one (reserved email domains and, where a country has one, fictional phone numbers). `user --unique email` (or `username`) makes no two users share the key; the batch is still the same for any `--threads`. `user --device` adds the device of each user's last sign-in as `device`, on the account's platform and locale; nothing else about the user changes.

`device --platform` takes a platform key (`android-phone`, `ios-phone`, `windows-desktop`, `macos-desktop`, `ipad`, `android-tablet`, `linux-desktop`, `chromeos-desktop`, `other`) and `--browser` a browser key (`chrome`, `safari`, `edge`, `firefox`, `samsung`, `opera`, `yandex`); a browser that does not run on the platform is an error. `--reference-date` is the date the models and versions are those of (2021-01-01 or later; 2023-07-01 by default), `--ip v4`, `v6` or `any` (by the country's IPv6 share) the address family, `--locale` a BCP 47 tag for the `Accept-Language` header, and `--safe` takes addresses from the documentation ranges. `--unique ip` makes no two devices share an address.

## Batches

| Option | |
| --- | --- |
| `--count N` | Number of items (default 1). |
| `--seed HEX` | The batch seed, 1 to 16 hexadecimal digits. Without it a random seed is drawn and printed to standard error, so the run can be repeated. |
| `--first N` | Index of the first item (default 0). |
| `--threads N` | Worker threads (default 1; `0` is the number of cores). |
| `--format` | `jsonl` (default), `csv` or `json`. |
| `--out FILE` | Write to a file instead of standard output. |

Item *i* is exactly the single call with the seed derived from the batch seed and *i*. So:

- **The output does not depend on `--threads`.** Byte for byte.
- **A range is generated directly.** `--first 1000000 --count 10` is items 1,000,000 to 1,000,009 of the same batch, without generating the first million.
- **Splits join.** Two runs `--count 500 --first 0` and `--count 500 --first 500` together equal `--count 1000`, so a large job can be spread over machines.
- **An item replays alone** from the batch seed and its index.
- **The same seed gives the same bytes on every platform**: Linux, macOS and Windows, x86-64 and ARM64.

`csv` flattens each result's JSON into dotted columns (`country.cca2`, `birth.date`); an array is one cell holding its JSON. The columns come from the first record. `json` writes one array with an item per line. `jsonl` and `csv` stream, so a batch of millions never has to fit in memory.

On a desktop, `persona` runs at about 17,000 per second on one thread and 60,000 per second on all cores.

## Data

The `lite` tier is built in. `--tier full` loads each library's full data from its checkout: give `--data-dir` the folder that holds the libraries' repositories (`<DIR>/name-generator/resources/full`, `<DIR>/city-generator/resources/full`, ...). The full tier is much larger (names alone are 41 MB) and loads slower. The data's sources and licences are in each library's `doc/data.md`; the built-in data of `persona` is described in [persona-generator](https://github.com/dasmig/persona-generator).

`persona` leaves `admin1` (the city's state or province) empty with the lite tier; the full tier fills it.

A seed replays the same result only with the same data and library versions. `dasmig-gen --version` prints the program and core versions, and every result carries its own `signature` with the seed and a hash of the data.

## Building

The libraries are header-only siblings, so a build needs them next to this repository:

```
git clone https://github.com/dasmig/generator-cli
# clone the seventeen repositories it uses beside it (address, biodata, birth, city, country, device, email,
# entity, interest, name, nickname, occupation, persona, phone, relationship, trait, user: dasmig/<name>-generator)
cd generator-cli
make            # build/dasmig-gen (clang++; CXX=g++-14 for GCC)
make test       # python3 tests/test_cli.py
```

CI builds against tagged library releases (`.github/actions/checkout-libs`) and runs the tests on all four platforms; the known-output hashes in `tests/test_cli.py` must match on each. A tag `vX.Y.Z` publishes the binaries.

MIT licence. The data the libraries embed has its own terms; see their repositories.
