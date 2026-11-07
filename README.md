# dasmig-gen

Generate fictional people, places and data in bulk from the command line: JSON Lines, CSV or JSON, with a seed so a run can be repeated exactly, on any machine.

`dasmig-gen` is a single executable over the [dasmig generator libraries](https://github.com/dasmig) (C++23). Their `lite` data is built in, so there is nothing to install or download.

```
dasmig-gen persona --count 100000 --seed 1f --format jsonl --threads 0 --out people.jsonl
dasmig-gen persona --country BR --sex female --age 25-40 --count 1000 --format csv --out brazil.csv
dasmig-gen name --country DE --gender f --count 20
dasmig-gen city --country JP --count 50 --uniform --format json
dasmig-gen weather --lat 48.85 --lon 2.35 --start 2024-01-01 --days 366 --seed 7 --format csv
dasmig-gen holidays --country BR --year 2025 --format csv
```

Binaries for Linux (x86-64 and ARM64), macOS (ARM64) and Windows (x86-64) are attached to each [release](https://github.com/dasmig/generator-cli/releases), with `SHA256SUMS`.

## Kinds

| Kind | What it generates | Request options |
| --- | --- | --- |
| `persona` | A complete character: name, country, city, birth, body, personality, interests, job | `--country --sex --age --reference-date --traits --relations --up --down --stepparents --home --identity --finance --safe` and their options |
| `name`, `surname` | A given and family name, or a family name | `--country --gender` |
| `nickname` | A nickname, from `--name` or the word lists | `--name` |
| `city` | A city (GeoNames) | `--country --uniform` |
| `country` | Country facts | `--region --uniform` |
| `birth` | A birth date and age | `--country --sex --age --reference-date --uniform` |
| `biodata` | Height, weight, eyes, hair, skin, blood type | `--country --sex --age` |
| `trait` | A personality | `--traits` |
| `interest` | Religion, hobbies and media | `--country --sex --age` |
| `occupation` | Employment, education, job and income | `--country --sex --age` |
| `user` | A user record: name, birth, address, phone, email and an account (username, password, dates, device, locale) | `--country --sex --age --reference-date --safe --unique --device --identity --finance` and their options |
| `family` | The people around a person: parents, siblings, partners and children, with ages, deaths and names by country | `--country --sex --age --reference-date --uniform --up --down --stepparents --home` |
| `device` | A device whose model, OS, browser, user agent, client hints, screen and IP address fit together at a date | `--country --reference-date --platform --browser --ip --locale --safe --unique --uniform` |
| `weather` | Days of weather at a place: temperatures, precipitation and its form, snow on the ground, thunder, drizzle, fog, cloud, humidity, wind and gusts, sun and solar radiation, and the day in a word | `--lat --lon --city --country --start --days --units --elevation --warming --hourly --historical --no-spatial` |
| `locale` | A locale tag (`en-CA`, `fr-CA`) drawn by speakers, with its CLDR date, time, number and currency formats, symbols, currency, week and measurement system | `--country --language --uniform` |
| `timezone` | An IANA time zone drawn by population, with its UTC offset, abbreviation and daylight time at a date | `--country --reference-date --uniform` |
| `holidays` | A country's public holidays in a year, one row per day off (a lookup, not a batch) | `--country --year --optional --observances` |
| `identity` | National numbers and documents, as test data: a number with what it encodes, a passport with its MRZ | `--country --sex --scheme --document --reference-date --safe --unique --uniform --with-name --age` |
| `finance` | Bank accounts and payment cards, as test data | `--country --accounts --cards --network --card-source --reference-date --safe --unique --uniform --with-name --sex --age` |
| `validate` | Whether a number, an account or a card is well formed (a lookup, not a batch) | `SCHEME TEXT --reference-date` |

What a request leaves out is drawn. `--age` is `N` or `MIN-MAX` (completed years, 0-120; `biodata`, `interest`, `occupation` and `family` take one age; `user` ages start at 13). Countries are ISO 3166-1 alpha-2 codes. The generator never reads the clock: pass `--reference-date YYYY-MM-DD` for ages as of a date.

`family --up N --down N` sets how many generations above and below the person to generate (1 each by default; 0 leaves them out), and the person is the one `--country`, `--sex` and `--age` describe.

`persona --relations` also draws the persona's family (parents, siblings, partners and children) into a `relatives` field, with `--up`, `--down`, `--stepparents` and `--home` as for `family` (`--stepparents` adds a parent's new partner, the children the parent had with them and their own children from before as `stepsibling`, `--home` adds who the person lives with as `household`; both are off by default and need `--relations` for a persona). Nothing else about the persona changes, so the same seed gives the same persona with and without it.

`user --safe` makes contacts that reach no one (reserved email domains and, where a country has one, fictional phone numbers). `user --unique email` (or `username`) makes no two users share the key; the batch is still the same for any `--threads`. `user --device` adds the device of each user's last sign-in as `device`, on the account's platform and locale; nothing else about the user changes.

`weather` items are `--days` consecutive days (1 by default, up to 36,600) at one place from `--start` (2023-07-01 by default), one row per day. The place is `--lat` and `--lon`, or with `--city` a city drawn by population (in the `--country` given), whose name goes in each day as `city`, or else a point drawn by land area. A day depends only on the seed, the place and the date, so the 18th of a series from the 15th is the same as `--start` on the 18th with the same item seed. `--units imperial` gives °F, inches and mph; `--elevation` (metres) and `--warming` (°C) move the temperatures; `--hourly` adds each day's 24 hours as `hours` (one CSV cell). `--historical` draws each month of 1991-2020 as it was (a cold January cold) and needs `weather-generator/resources/history/history.tsv` under `--data-dir` (the current folder by default), since the 23 MB table is not built in. On one seed nearby places share their weather (a rainy day in Paris is likely rainy in London); `--no-spatial` gives each climate cell weather of its own. A point more than 100 km from land is an error. See [weather-generator](https://github.com/dasmig/weather-generator).

`device --platform` takes a platform key (`android-phone`, `ios-phone`, `windows-desktop`, `macos-desktop`, `ipad`, `android-tablet`, `linux-desktop`, `chromeos-desktop`, `other`) and `--browser` a browser key (`chrome`, `safari`, `edge`, `firefox`, `samsung`, `opera`, `yandex`); a browser that does not run on the platform is an error. `--reference-date` is the date the models and versions are those of (2021-01-01 or later; 2023-07-01 by default), `--ip v4`, `v6` or `any` (by the country's IPv6 share) the address family, `--locale` a BCP 47 tag for the `Accept-Language` header, and `--safe` takes addresses from the documentation ranges. `--unique ip` makes no two devices share an address.

`locale`, `timezone` and `country` with one seed draw the same country, so a locale and a zone can be paired with a country. `locale --language fr` gives only French tags, and with no `--country` it draws the country by French speakers. `timezone --reference-date` is the date whose offset and abbreviation are reported (2023-07-01 by default; Berlin is `CEST` then and `CET` on 2024-01-15). The built-in rules start in 2000, and `--tier full` goes back to 1970.

`identity` and `finance` give test data; read [the limits](#limits). `identity --scheme br-cpf,br-rg` picks the schemes, by default the country's primary, and `--document passport,id_card` adds documents. A document has no machine-readable zone and a card no holder without a name, so `--with-name` draws a name and a birth first, passes them in and adds the name as `name` (with it, `--age` applies, 18-80 by default for `finance`). A country drawn with `--with-name` is one with names. `finance --accounts current,savings` (or `none`) and `--cards debit,credit,prepaid,any` say what to make, `--network visa` the network, and `--card-source network_range`, `test_number` or `wrong_check` where the numbers come from. `--unique number` (`identity`), `--unique account` or `card` (`finance`) make no two items share it; an item without one is never a duplicate.

`user` and `persona` take `--identity` and `--finance` to add those parts, with the same options, from their own birth date, sex, names and address or city. Nothing else about them changes. A persona's accounts and cards are a chance, by the country's Global Findex shares, and a user's are as asked.

`validate SCHEME TEXT` says whether a text is well formed: `validate br-cpf 111.444.777-35`, `validate iban-de "DE89 3704 0044 0532 0130 00"`, or `validate card 4242424242424242`, which also lists the networks the number could belong to. It prints `valid`, and exits 0 for valid and 3 for not, 1 for a scheme neither library has. `--reference-date` applies to numbers that encode a birth date. Valid means the structure and check digits are right: nothing says the number exists or belongs to anyone.

`holidays --country BR --year 2025` lists the year's national holidays by date, with the name in the country's language and in English, a stable `key`, the `kind` and, where a weekend moves a day off, the `observed` date (the US's New Year's Day 2022 is observed on 2021-12-31). `--optional` adds days off for some only (Brazil's Carnival), and `--observances` adds days that are not days off. It is a lookup, so `--count`, `--seed`, `--first` and `--threads` are errors. Each country's years are python-holidays' span within 1970-2050, and a year outside it is an error. See [country-generator](https://github.com/dasmig/country-generator).

## Limits

These apply to `identity`, `finance`, `validate`, and `user` and `persona` with `--identity` or `--finance`.

- Without `--safe`, national numbers, accounts and cards are structurally valid and may match a real person's. Nothing is looked up, and nothing can tell.
- They are test data. Never use them to identify anyone, to open accounts, to pass an identity or payment check, or on a live payment system. Trying card numbers on a live system is card testing, which is fraud, whatever produced them.
- `--safe` means different things per scheme, and for many accounts it promises nothing. See [id-generator's limits](https://github.com/dasmig/id-generator#limits) and [finance-generator's limits](https://github.com/dasmig/finance-generator#limits).
- Documents and cards are data, not images.

## Batches

| Option | |
| --- | --- |
| `--count N` | Number of items (default 1). These options apply to every kind but `holidays`. |
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

`csv` flattens each result's JSON into dotted columns (`country.cca2`, `birth.date`); an array is one cell holding its JSON. The columns come from the first record. In `occupation` and `persona`, a job or income that is null still has its columns (`job.label`, `income.amount`, `income.currency`), left empty, so a first record without a job does not drop the later records' ones. `json` writes one array with an item per line. `jsonl` and `csv` stream, so a batch of millions never has to fit in memory.

On a desktop, `persona` runs at about 17,000 per second on one thread and 60,000 per second on all cores.

## Data

The `lite` tier is built in. `--tier full` loads each library's full data from its checkout: give `--data-dir` the folder that holds the libraries' repositories (`<DIR>/name-generator/resources/full`, `<DIR>/city-generator/resources/full`, ...). The full tier is much larger (names alone are 41 MB) and loads slower. The data's sources and licences are in each library's `doc/data.md`; the built-in data of `persona` is described in [persona-generator](https://github.com/dasmig/persona-generator).

`persona` leaves `admin1` (the city's state or province) empty with the lite tier; the full tier fills it.

A seed replays the same result only with the same data and library versions. `dasmig-gen --version` prints the program and core versions, and every result carries its own `signature` with the seed and a hash of the data.

## Building

The libraries are header-only siblings, so a build needs them next to this repository:

```
git clone https://github.com/dasmig/generator-cli
# clone the eighteen repositories it uses beside it (address, biodata, birth, city, country, device, email,
# entity, interest, name, nickname, occupation, persona, phone, relationship, trait, user, weather:
# dasmig/<name>-generator)
cd generator-cli
make            # build/dasmig-gen (clang++; CXX=g++-14 for GCC)
make test       # python3 tests/test_cli.py
```

CI builds against tagged library releases (`.github/actions/checkout-libs`) and runs the tests on all four platforms; the known-output hashes in `tests/test_cli.py` must match on each. A tag `vX.Y.Z` publishes the binaries.

MIT licence. The data the libraries embed has its own terms; see their repositories.
