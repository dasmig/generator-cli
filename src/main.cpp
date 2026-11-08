// dasmig-gen: a command line over the dasmig generator libraries.
//
//   dasmig-gen persona --count 100000 --seed 1f --format jsonl --threads 0
//
// Every kind generates a batch with the libraries' batch API, so the output is
// the same for any --threads, and any item replays on its own: item i is the
// single call with core::item_seed(seed, first + i).

#include <dasmig/biodatagen.hpp>
#include <dasmig/birthgen.hpp>
#include <dasmig/citygen.hpp>
#include <dasmig/countrygen.hpp>
#include <dasmig/devicegen.hpp>
#include <dasmig/fingen.hpp>
#include <dasmig/idgen.hpp>
#include <dasmig/interestgen.hpp>
#include <dasmig/namegen.hpp>
#include <dasmig/nicknamegen.hpp>
#include <dasmig/occupationgen.hpp>
#include <dasmig/personagen.hpp>
#include <dasmig/propertygen.hpp>
#include <dasmig/relgen.hpp>
#include <dasmig/traitgen.hpp>
#include <dasmig/usergen.hpp>
#include <dasmig/weathergen.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#ifndef DASMIG_GEN_VERSION
#define DASMIG_GEN_VERSION "3.9.0"
#endif

namespace
{

namespace core = dasmig::core;

struct usage_error : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

constexpr std::string_view help_text = R"(dasmig-gen: generate fictional people, places and data in bulk.

Usage: dasmig-gen KIND [options]

Kinds:
  persona     a complete character (name, country, city, birth, body, traits, interests, job)
  name        a given and family name          surname     a family name
  nickname    a nickname (from --name, or from word lists)
  city        a city (GeoNames)                country     country facts
  birth       a birth date and age             biodata     height, weight, eyes, hair, skin, blood type
  trait       a personality                    interest    religion, hobbies and media
  occupation  employment, education, job and income
  user        a user record (name, birth, address, phone, email, account)
  device      a device: model, OS, browser, user agent, screen and IP address
  family      the people around a person (parents, siblings, partners, children)
  weather     days of weather at a place (CRU TS normals, 1991-2020)
  locale      a locale tag with its CLDR formats (en-CA, fr-CA), drawn by speakers
  timezone    an IANA time zone with its offset, drawn by population
  holidays    a country's public holidays in a year (a lookup: one row per holiday,
              no batch options)
  identity    national numbers and documents, as test data: see Limits below
  finance     bank accounts and payment cards, as test data: see Limits below
  property    a household's home: dwelling type, tenure, rooms, floor area, year built
              and monthly cost
  validate    SCHEME TEXT: whether a number, an account or a card is well formed (a lookup,
              no batch options; exit 0 if valid, 3 if not). SCHEME is a scheme key (br-cpf,
              iban-de), or "card" for a card number with the networks it could belong to

Limits (identity, finance, and user and persona with --identity or --finance):
  Without --safe, numbers, accounts and cards are structurally valid and may match a real
  person's; nothing is looked up. They are test data: never use them to identify anyone, to
  open accounts, to pass an identity or payment check, or on a live payment system. Trying
  card numbers on a live system is card testing, which is fraud. --safe means different things
  per scheme and for many accounts promises nothing: see the id-generator and
  finance-generator documentation.

Batch options (every kind but holidays and validate):
  --count N          number of items (default 1)
  --seed HEX         batch seed, 1 to 16 hexadecimal digits (default random, printed to stderr)
  --first N          index of the first item (default 0); item i is the single call with
                     item_seed(seed, i), so ranges and splits give the same items
  --threads N        worker threads (default 1; 0 is the number of cores); the output
                     does not depend on it
  --format FMT       jsonl (default), csv or json
  --out FILE         write to FILE instead of standard output
  --tier lite|full   lite data is built in (default); full reads the libraries' resources
  --data-dir DIR     with --tier full: the folder holding the libraries' checkouts
                     (<DIR>/name-generator/resources, ...)

Request options (the ones a kind understands; the rest is drawn):
  --country CC       ISO 3166-1 alpha-2 code        (persona, name, surname, city, birth, biodata,
                                                     interest, occupation, user, family, device, locale,
                                                     timezone, property; holidays requires it)
  --sex male|female                                 (persona, birth, biodata, interest, occupation, user, family,
                                                     identity; finance with --with-name)
  --age N or MIN-MAX                                (persona, birth, biodata, interest, occupation, user;
                                                     biodata, interest, occupation and family take one age;
                                                     property one, the reference person's, 16 to 110)
  --reference-date YYYY-MM-DD   the date ages are computed at (persona, birth, user, family);
                     for property, no home is built after it;
                     for device, the date its models and versions are those of (2021-01-01 or later);
                     for timezone, the date of the offset reported (2000 or later; 1970 with --tier full)
  --traits N         number of personality traits   (persona, trait)
  --gender m|f|any   (name, surname)
  --name TEXT        full name to derive from       (nickname)
  --region NAME      UN geoscheme region            (country)
  --up N, --down N   generations above and below the person (default 1 each) (family; persona with --relations)
  --relations        also draw the persona's family (parents, siblings, partners, children) as "relatives" (persona)
  --stepparents      also a parent's new partner, as stepfather or stepmother, and their children (family; persona with --relations)
  --home             also who the person lives with, as "household" (family; persona with --relations)
  --uniform          uniform instead of population-weighted choice (city, country, birth, family, device,
                     locale, timezone, property)
  --language CODE    ISO 639 code: only that language's tags; with no --country, the country is
                     drawn by its speakers (locale)
  --safe             contacts that reach no one: reserved email domains, fictional phone ranges (user);
                     IP addresses from the documentation ranges (device, user --device);
                     numbers, documents, accounts and cards by the libraries' safe methods
                     (identity, finance, and user and persona with --identity or --finance)
  --unique KEY       no two items share the key: email or username (user), ip (device),
                     number (identity), account or card (finance)
  --device           also the device of the user's last sign-in, as "device" (user)
  --identity         also the person's numbers and documents, as "identity" (user, persona)
  --finance          also the person's accounts and cards, as "finance" (user, persona; a
                     persona's are a chance, by the country's Global Findex shares)
  --property         also the persona's home and its household's earnings, as "home" and
                     "household_income"; --area, --type and --tenure then apply (persona)
  --with-name        draw a name and a birth first and pass them in, which is what gives a document
                     its MRZ and a card its holder; adds "name", and --age applies (identity, finance)
  --scheme A,B       the number schemes (br-cpf); default the country's primary (identity)
  --document A,B     passport, id_card; default none (identity)
  --accounts A,B     current, savings, or none; default one current (finance)
  --cards A,B        debit, credit, prepaid, any; default none (finance)
  --network NAME     the card network (visa); default drawn for the country (finance)
  --card-source S    network_range, test_number or wrong_check; default network_range,
                     test_number with --safe (finance)
  --area A           city, town or rural (property; persona with --property)
  --type T           detached, semi_detached, flat_small, flat_large, informal or other
                     (property; persona with --property)
  --tenure T         owner_outright, owner_mortgage, tenant_market or tenant_reduced
                     (property; persona with --property)
  --household-size N, --household-type T, --children N
                     the household: its size, its type (one_person, couple, nuclear,
                     single_parent, with_parents, multi_generation, other) and the members
                     under 18; the rest is drawn to fit (property)
  --income AMOUNT    the household's gross annual income in the country's currency, which
                     gives the home its monthly cost; needs --country (property)
  --platform KEY     android-phone, ios-phone, windows-desktop, macos-desktop, ipad,
                     android-tablet, linux-desktop, chromeos-desktop or other (device)
  --browser KEY      chrome, safari, edge, firefox, samsung, opera or yandex (device)
  --ip v4|v6|any     the IP family (device; default any, by the country's IPv6 share)
  --locale TAG       BCP 47 locale for the Accept-Language header (device)

Weather options (an item is --days days at one place, one row per day):
  --lat DEG, --lon DEG   the place (default: drawn by land area); set both or neither
  --city             at a city drawn by population instead (with --country, in that country);
                     each day gets the city as "city"
  --start YYYY-MM-DD the first day (default 2023-07-01)
  --days N           days per item, 1 to 36600 (default 1)
  --units metric|imperial    °C, mm, cm, m/s (default) or °F, inches, mph
  --elevation M      metres, -500 to 9000 (default: the climate cell's)
  --warming C        °C added to every temperature, -10 to 10
  --hourly           also each day's 24 hours, as "hours"
  --historical       each month as it was in 1991-2020 (CRU TS), not around the normals; every
                     day must fall in 1991-2020, and weather-generator/resources/history/history.tsv
                     is read from --data-dir (default the current folder)
  --no-spatial       each climate cell's own weather; by default nearby places share their
                     weather on one seed (items at different places agree as neighbours do)

Holidays options (dasmig-gen holidays --country BR --year 2025):
  --year YYYY        the year (required); each country has its own span within 1970-2050
  --optional         also days off for some only (Brazil's Carnival, government-only days)
  --observances      also days that are not days off (Mother's Day)

Other:
  --help, --version
)";

std::uint64_t parse_hex_seed(std::string_view text)
{
    if (text.starts_with("0x") || text.starts_with("0X")) text.remove_prefix(2);
    if (text.empty() || text.size() > 16) throw usage_error("--seed must be 1 to 16 hexadecimal digits");
    std::uint64_t value = 0;
    for (const char c : text)
    {
        value <<= 4U;
        if (c >= '0' && c <= '9') value |= static_cast<std::uint64_t>(c - '0');
        else if (c >= 'a' && c <= 'f') value |= static_cast<std::uint64_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') value |= static_cast<std::uint64_t>(c - 'A' + 10);
        else throw usage_error("--seed must be 1 to 16 hexadecimal digits");
    }
    return value;
}

std::uint64_t parse_count(std::string_view text, const std::string& option)
{
    if (text.empty() || text.size() > 19) throw usage_error(option + " must be a whole number");
    std::uint64_t value = 0;
    for (const char c : text)
    {
        if (c < '0' || c > '9') throw usage_error(option + " must be a whole number");
        value = (value * 10U) + static_cast<std::uint64_t>(c - '0');
    }
    return value;
}

std::string hex16(std::uint64_t value)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i, value >>= 4U) out[static_cast<std::size_t>(i)] = digits[value & 0xFU];
    return out;
}

dasmig::sex parse_sex(const std::string& text)
{
    if (text == "male" || text == "m") return dasmig::sex::male;
    if (text == "female" || text == "f") return dasmig::sex::female;
    throw usage_error("--sex must be male or female");
}

dasmig::age_range parse_ages(const std::string& text)
{
    const auto dash = text.find('-');
    const auto lo = parse_count(text.substr(0, dash), "--age");
    const auto hi = dash == std::string::npos ? lo : parse_count(text.substr(dash + 1), "--age");
    if (lo > 120 || hi > 120 || lo > hi) throw usage_error("--age must be N or MIN-MAX with 0 <= MIN <= MAX <= 120");
    return {.min = static_cast<std::uint8_t>(lo), .max = static_cast<std::uint8_t>(hi)};
}

unsigned parse_single_age(const std::string& text)
{
    const auto ages = parse_ages(text);
    if (ages.min != ages.max) throw usage_error("this kind takes one age, not a range");
    return ages.min;
}

std::chrono::year_month_day parse_date(const std::string& text, const std::string& option = "--reference-date")
{
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') throw usage_error(option + " must be YYYY-MM-DD");
    const auto year = static_cast<int>(parse_count(text.substr(0, 4), option));
    const auto month = static_cast<unsigned>(parse_count(text.substr(5, 2), option));
    const auto day = static_cast<unsigned>(parse_count(text.substr(8, 2), option));
    const std::chrono::year_month_day date{std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day}};
    if (!date.ok() || year < 1900 || year > 2100) throw usage_error(option + " is not a calendar date from 1900 to 2100");
    return date;
}

double parse_decimal(const std::string& text, const std::string& option)
{
    const auto value = core::detail::parse_number<double>(text);
    if (!value) throw usage_error(option + " must be a number");
    return *value;
}

struct arguments
{
    std::string kind;
    std::uint64_t count{1};
    std::uint64_t first{0};
    std::size_t threads{1};
    std::optional<std::uint64_t> seed;
    std::string format{"jsonl"};
    std::string out;
    std::string tier{"lite"};
    std::string data_dir;
    std::map<std::string, std::string> request; // --country, --sex, ... (without the dashes)
    std::set<std::string> flags;                // --uniform
    std::set<std::string> batch_given;          // --count, --seed, ... as given (holidays takes none)
    std::vector<std::string> operands;          // validate's scheme and text
};

const std::set<std::string> value_options{"country", "sex",   "age",    "reference-date", "traits",    "gender",
                                          "name",    "region", "unique", "up",           "down",      "platform",
                                          "browser", "ip",    "locale", "lat",          "lon",       "start",
                                          "days",    "units", "elevation", "warming",     "language",  "year",
                                          "scheme",  "document", "accounts", "cards",     "network",   "card-source",
                                          "area",    "type",  "tenure", "household-size", "household-type", "children",
                                          "income"};
const std::set<std::string> kinds{"persona", "name",     "surname",    "nickname", "city",   "country",
                                  "birth",   "biodata",  "trait",      "interest", "occupation", "user",
                                  "family",  "device",   "weather",    "locale",   "timezone",   "holidays",
                                  "identity", "finance", "validate", "property"};

arguments parse_arguments(int argc, char** argv)
{
    arguments args;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (!arg.starts_with("--"))
        {
            if (args.kind.empty()) args.kind = arg;
            else if (args.kind == "validate" && args.operands.size() < 2) args.operands.push_back(arg);
            else throw usage_error("unexpected argument '" + arg + "'");
            continue;
        }
        std::string name = arg.substr(2);
        std::optional<std::string> inline_value;
        if (const auto eq = name.find('='); eq != std::string::npos)
        {
            inline_value = name.substr(eq + 1);
            name.resize(eq);
        }
        if (name == "uniform" || name == "safe" || name == "relations" || name == "stepparents" || name == "home" ||
            name == "device" || name == "city" || name == "hourly" || name == "historical" ||
            name == "no-spatial" || name == "optional" || name == "observances" || name == "identity" ||
            name == "finance" || name == "with-name" || name == "property")
        {
            args.flags.insert(name);
            continue;
        }
        const auto value = [&]() -> std::string
        {
            if (inline_value) return *inline_value;
            if (i + 1 >= argc) throw usage_error("--" + name + " needs a value");
            return argv[++i];
        };
        if (name == "count" || name == "first" || name == "threads" || name == "seed") args.batch_given.insert(name);
        if (name == "count") args.count = parse_count(value(), "--count");
        else if (name == "first") args.first = parse_count(value(), "--first");
        else if (name == "threads") args.threads = static_cast<std::size_t>(parse_count(value(), "--threads"));
        else if (name == "seed") args.seed = parse_hex_seed(value());
        else if (name == "format") args.format = value();
        else if (name == "out") args.out = value();
        else if (name == "tier") args.tier = value();
        else if (name == "data-dir") args.data_dir = value();
        else if (value_options.contains(name)) args.request[name] = value();
        else throw usage_error("unknown option '--" + name + "'");
    }
    if (args.kind.empty()) throw usage_error("missing KIND");
    if (!kinds.contains(args.kind)) throw usage_error("unknown kind '" + args.kind + "'");
    if (args.format != "jsonl" && args.format != "csv" && args.format != "json")
        throw usage_error("--format must be jsonl, csv or json");
    if (args.tier != "lite" && args.tier != "full") throw usage_error("--tier must be lite or full");
    return args;
}

// The options a kind understands; any other request option is an error.
const std::map<std::string, std::set<std::string>> accepted{
    {"persona", {"country", "sex", "age", "reference-date", "traits", "uniform", "relations", "up", "down", "stepparents", "home",
                 "safe", "identity", "finance", "scheme", "document", "accounts", "cards", "network", "card-source",
                 "property", "area", "type", "tenure"}},
    {"name", {"country", "gender"}},
    {"surname", {"country", "gender"}},
    {"nickname", {"name"}},
    {"city", {"country", "uniform"}},
    {"country", {"region", "uniform"}},
    {"birth", {"country", "sex", "age", "reference-date", "uniform"}},
    {"biodata", {"country", "sex", "age"}},
    {"trait", {"traits"}},
    {"interest", {"country", "sex", "age"}},
    {"occupation", {"country", "sex", "age"}},
    {"user", {"country", "sex", "age", "reference-date", "safe", "unique", "device",
              "identity", "finance", "scheme", "document", "accounts", "cards", "network", "card-source"}},
    {"family", {"country", "sex", "age", "reference-date", "uniform", "up", "down", "stepparents", "home"}},
    {"device", {"country", "reference-date", "platform", "browser", "ip", "locale", "safe", "unique", "uniform"}},
    {"weather", {"lat", "lon", "city", "country", "start", "days", "units", "elevation", "warming", "hourly", "historical",
                 "no-spatial"}},
    {"locale", {"country", "language", "uniform"}},
    {"timezone", {"country", "reference-date", "uniform"}},
    {"holidays", {"country", "year", "optional", "observances"}},
    {"identity", {"country", "sex", "age", "reference-date", "scheme", "document", "safe", "unique", "uniform", "with-name"}},
    {"finance", {"country", "sex", "age", "reference-date", "accounts", "cards", "network", "card-source", "safe", "unique",
                 "uniform", "with-name"}},
    {"validate", {"reference-date"}},
    {"property", {"country", "area", "type", "tenure", "household-size", "household-type", "children", "age", "income",
                  "reference-date", "uniform"}}};

void check_request_options(const arguments& args)
{
    const auto& ok = accepted.at(args.kind);
    for (const auto& [name, _] : args.request)
        if (!ok.contains(name)) throw usage_error("--" + name + " does not apply to " + args.kind);
    if (const auto unique = args.request.find("unique"); unique != args.request.end())
    {
        if (args.kind == "device" && unique->second != "ip") throw usage_error("--unique must be ip for device");
        if (args.kind == "user" && unique->second != "email" && unique->second != "username")
            throw usage_error("--unique must be email or username");
        if (args.kind == "identity" && unique->second != "number") throw usage_error("--unique must be number for identity");
        if (args.kind == "finance" && unique->second != "account" && unique->second != "card")
            throw usage_error("--unique must be account or card for finance");
    }
    if (const auto ip = args.request.find("ip");
        ip != args.request.end() && ip->second != "v4" && ip->second != "v6" && ip->second != "any")
        throw usage_error("--ip must be v4, v6 or any");
    for (const auto& name : args.flags)
        if (!ok.contains(name)) throw usage_error("--" + name + " does not apply to " + args.kind);
    if (args.kind == "weather")
    {
        if (args.request.contains("lat") != args.request.contains("lon")) throw usage_error("--lat and --lon go together");
        if (args.flags.contains("city") && args.request.contains("lat"))
            throw usage_error("--city draws the place; it does not go with --lat and --lon");
        if (args.request.contains("country") && !args.flags.contains("city"))
            throw usage_error("--country needs --city for weather");
        if (const auto units = args.request.find("units");
            units != args.request.end() && units->second != "metric" && units->second != "imperial")
            throw usage_error("--units must be metric or imperial");
    }
    if (args.kind == "holidays")
    {
        // A lookup, not a draw: there is nothing to seed, count or split.
        for (const auto& name : args.batch_given)
            throw usage_error("--" + name + " does not apply to holidays (a lookup: one row per holiday)");
        if (!args.request.contains("country")) throw usage_error("holidays needs --country");
        if (!args.request.contains("year")) throw usage_error("holidays needs --year");
    }
    if (args.kind == "validate")
    {
        for (const auto& name : args.batch_given)
            throw usage_error("--" + name + " does not apply to validate (a lookup)");
        if (args.operands.size() != 2) throw usage_error("validate needs a scheme and a text: validate SCHEME TEXT");
    }
    if ((args.kind == "identity" || args.kind == "finance") && !args.flags.contains("with-name"))
    {
        if (args.request.contains("age")) throw usage_error("--age needs --with-name for " + args.kind);
        if (args.kind == "finance" && args.request.contains("sex")) throw usage_error("--sex needs --with-name for finance");
    }
    if (args.kind == "persona" || args.kind == "user")
    {
        for (const char* name : {"scheme", "document"})
            if (args.request.contains(name) && !args.flags.contains("identity"))
                throw usage_error(std::string{"--"} + name + " needs --identity for " + args.kind);
        for (const char* name : {"accounts", "cards", "network", "card-source"})
            if (args.request.contains(name) && !args.flags.contains("finance"))
                throw usage_error(std::string{"--"} + name + " needs --finance for " + args.kind);
    }
    if (args.kind == "persona")
        for (const char* name : {"area", "type", "tenure"})
            if (args.request.contains(name) && !args.flags.contains("property"))
                throw usage_error(std::string{"--"} + name + " needs --property for persona");
    if (args.kind == "property" && args.request.contains("income") && !args.request.contains("country"))
        throw usage_error("--income needs --country (the amount is in its currency)");
    if (args.kind == "persona" && args.flags.contains("safe") && !args.flags.contains("identity") &&
        !args.flags.contains("finance"))
        throw usage_error("--safe needs --identity or --finance for persona");
    if (args.kind == "persona" && !args.flags.contains("relations"))
    {
        for (const char* name : {"up", "down"})
            if (args.request.contains(name)) throw usage_error(std::string{"--"} + name + " needs --relations for persona");
        for (const char* name : {"stepparents", "home"})
            if (args.flags.contains(name)) throw usage_error(std::string{"--"} + name + " needs --relations for persona");
    }
}

std::optional<std::string> get(const arguments& args, const std::string& key)
{
    const auto it = args.request.find(key);
    if (it == args.request.end()) return std::nullopt;
    return it->second;
}

std::string lower(std::string text)
{
    std::ranges::transform(text, text.begin(),
                           [](unsigned char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c); });
    return text;
}

std::string upper(std::string text)
{
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

// A JSON array of the items, one per line.
class json_array_writer
{
  public:
    explicit json_array_writer(std::ostream& out) : _out{&out} {}

    void add(const std::string& json)
    {
        *_out << (_empty ? "[\n" : ",\n") << json;
        _empty = false;
    }

    void finish() { *_out << (_empty ? "[]\n" : "\n]\n"); }

  private:
    std::ostream* _out;
    bool _empty{true};
};

// An object member that is sometimes null, and the columns it has when it is
// not ("job" and "job.key", "job.label", ...).
struct nullable_object
{
    std::string column;
    std::vector<std::string> columns;
};

// The output format, and for CSV the objects a kind writes as null at times.
struct output_format
{
    std::string name;
    std::vector<nullable_object> nullable;
};

// CSV as core::csv_writer writes it (dotted columns from the first record,
// later records matched by name, the same quoting), except that a null member
// listed in `nullable` is written as its columns, empty. Without this, a first
// record with no job or income (a student) would give one "job" column and drop
// every later record's job and income.
class csv_table_writer
{
  public:
    csv_table_writer(std::ostream& out, const std::vector<nullable_object>& nullable)
        : _out{&out}, _nullable{&nullable} {}

    void write_json(std::string_view json)
    {
        auto cells = core::flatten_json(json);
        if (!_nullable->empty()) cells = expand(std::move(cells));
        if (!_header_written)
        {
            for (const auto& cell : cells) _columns.push_back(cell.first);
            for (std::size_t i = 0; i < _columns.size(); ++i) _position.emplace(_columns[i], i);
            write_row(_columns);
            _header_written = true;
        }
        std::vector<std::string> row(_columns.size());
        const bool in_order = std::ranges::equal(cells, _columns, {}, &cell_list::value_type::first);
        for (std::size_t i = 0; i < cells.size(); ++i)
        {
            if (in_order) row[i] = std::move(cells[i].second);
            else if (const auto it = _position.find(cells[i].first); it != _position.end())
                row[it->second] = std::move(cells[i].second);
        }
        write_row(row);
    }

  private:
    using cell_list = std::vector<std::pair<std::string, std::string>>;

    cell_list expand(cell_list cells) const
    {
        cell_list out;
        out.reserve(cells.size());
        for (auto& cell : cells)
        {
            const auto it = std::ranges::find(*_nullable, cell.first, &nullable_object::column);
            if (it != _nullable->end() && cell.second.empty())
                for (const auto& column : it->columns) out.emplace_back(column, std::string{});
            else
                out.push_back(std::move(cell));
        }
        return out;
    }

    void write_row(const std::vector<std::string>& fields)
    {
        for (std::size_t i = 0; i < fields.size(); ++i)
        {
            if (i > 0) _out->put(',');
            const auto& field = fields[i];
            if (field.find_first_of(",\"\r\n") == std::string::npos)
            {
                *_out << field;
                continue;
            }
            _out->put('"');
            for (const char c : field)
            {
                if (c == '"') _out->put('"');
                _out->put(c);
            }
            _out->put('"');
        }
        _out->put('\n');
    }

    std::ostream* _out;
    const std::vector<nullable_object>* _nullable;
    std::vector<std::string> _columns;
    std::map<std::string, std::size_t, std::less<>> _position;
    bool _header_written{false};
};

// The members of an occupation that are null at times (job, income), with the
// columns they have when they are not, under `prefix` ("occupation." in a
// persona). The columns are read from a real result, an employed adult in the
// United States, so they follow occupation-generator's JSON.
std::vector<nullable_object> occupation_nullable(const std::string& prefix)
{
    dasmig::occupation_request request;
    request.country = "US";
    request.age = 35;
    request.status = dasmig::employment_status::employed;
    const auto cells = core::flatten_json(dasmig::ocg::instance().get_occupation(request, 0).to_json());
    std::vector<nullable_object> out;
    for (const std::string member : {"job", "income"})
    {
        nullable_object object{.column = prefix + member, .columns = {}};
        for (const auto& cell : cells)
            if (cell.first.starts_with(member + ".")) object.columns.push_back(prefix + cell.first);
        if (object.columns.empty()) throw std::logic_error("the prototype occupation has no " + member);
        out.push_back(std::move(object));
    }
    return out;
}

// An item's JSON with the key a unique batch compares.
struct keyed_json
{
    std::string json;
    std::string key;
};

// Generates the batch with `make(item_seed)`, which returns the item as JSON
// text, so serializing runs on the worker threads and the calling thread only
// writes. Items stream to the writer for the format in index order.
template <typename Make>
void emit(const output_format& format, std::ostream& out, const core::batch_options& options,
          std::uint64_t seed, const Make& make)
{
    if (format.name == "jsonl")
    {
        core::jsonl_writer writer{out};
        core::for_each_batch(options, seed, make,
                             [&writer](std::uint64_t, const std::string& json) { writer.write_line(json); });
    }
    else if (format.name == "csv")
    {
        csv_table_writer writer{out, format.nullable};
        core::for_each_batch(options, seed, make,
                             [&writer](std::uint64_t, const std::string& json) { writer.write_json(json); });
    }
    else
    {
        json_array_writer writer{out};
        core::for_each_batch(options, seed, make,
                             [&writer](std::uint64_t, const std::string& json) { writer.add(json); });
        writer.finish();
    }
}

// The same as emit(), for items that are unique by a key (core::unique_rule).
template <typename Make>
void emit_keyed(const output_format& format, std::ostream& out, const core::batch_options& options,
                std::uint64_t seed, const Make& make)
{
    if (format.name == "jsonl")
    {
        core::jsonl_writer writer{out};
        core::for_each_batch(options, seed, make,
                             [&writer](std::uint64_t, const keyed_json& item) { writer.write_line(item.json); });
    }
    else if (format.name == "csv")
    {
        csv_table_writer writer{out, format.nullable};
        core::for_each_batch(options, seed, make,
                             [&writer](std::uint64_t, const keyed_json& item) { writer.write_json(item.json); });
    }
    else
    {
        json_array_writer writer{out};
        core::for_each_batch(options, seed, make,
                             [&writer](std::uint64_t, const keyed_json& item) { writer.add(item.json); });
        writer.finish();
    }
}

// The same as emit(), for items of several rows (a weather series): `make`
// returns the rows' JSON, written in order.
template <typename Make>
void emit_rows(const output_format& format, std::ostream& out, const core::batch_options& options,
               std::uint64_t seed, const Make& make)
{
    using rows = std::vector<std::string>;
    if (format.name == "jsonl")
    {
        core::jsonl_writer writer{out};
        core::for_each_batch(options, seed, make, [&writer](std::uint64_t, const rows& items)
                             { for (const auto& json : items) writer.write_line(json); });
    }
    else if (format.name == "csv")
    {
        csv_table_writer writer{out, format.nullable};
        core::for_each_batch(options, seed, make, [&writer](std::uint64_t, const rows& items)
                             { for (const auto& json : items) writer.write_json(json); });
    }
    else
    {
        json_array_writer writer{out};
        core::for_each_batch(options, seed, make, [&writer](std::uint64_t, const rows& items)
                             { for (const auto& json : items) writer.add(json); });
        writer.finish();
    }
}

// Rows that are not a batch (a lookup such as holidays), in the same formats.
void emit_list(const output_format& format, std::ostream& out, const std::vector<std::string>& rows)
{
    if (format.name == "jsonl")
    {
        core::jsonl_writer writer{out};
        for (const auto& json : rows) writer.write_line(json);
    }
    else if (format.name == "csv")
    {
        csv_table_writer writer{out, format.nullable};
        for (const auto& json : rows) writer.write_json(json);
    }
    else
    {
        json_array_writer writer{out};
        for (const auto& json : rows) writer.add(json);
        writer.finish();
    }
}

// The parts of a comma-separated list.
std::vector<std::string> comma_list(const std::string& text)
{
    std::vector<std::string> parts;
    std::size_t from = 0;
    while (true)
    {
        const auto comma = text.find(',', from);
        parts.push_back(text.substr(from, comma - from));
        if (comma == std::string::npos) return parts;
        from = comma + 1;
    }
}

// --scheme, --document, --safe and --uniform as a request for numbers and documents.
dasmig::identity_request identity_options(const arguments& args)
{
    dasmig::identity_request request;
    if (const auto schemes = get(args, "scheme")) request.schemes = comma_list(*schemes);
    if (const auto documents = get(args, "document"))
        for (const auto& part : comma_list(*documents))
        {
            if (part == "passport") request.documents.push_back(dasmig::id_document_kind::passport);
            else if (part == "id_card") request.documents.push_back(dasmig::id_document_kind::id_card);
            else throw usage_error("--document must list passport or id_card, not '" + part + "'");
        }
    request.safe = args.flags.contains("safe");
    request.weighted = !args.flags.contains("uniform");
    return request;
}

// --accounts, --cards, --network, --card-source, --safe and --uniform as a
// request for accounts and cards.
dasmig::finance_request finance_options(const arguments& args)
{
    dasmig::finance_request request;
    if (const auto accounts = get(args, "accounts"))
    {
        request.accounts.clear();
        if (*accounts != "none")
            for (const auto& part : comma_list(*accounts))
            {
                if (part == "current") request.accounts.push_back(dasmig::account_kind::current);
                else if (part == "savings") request.accounts.push_back(dasmig::account_kind::savings);
                else throw usage_error("--accounts must list current or savings, or be none, not '" + part + "'");
            }
    }
    if (const auto cards = get(args, "cards"))
        for (const auto& part : comma_list(*cards))
        {
            if (part == "debit") request.cards.emplace_back(dasmig::card_kind::debit);
            else if (part == "credit") request.cards.emplace_back(dasmig::card_kind::credit);
            else if (part == "prepaid") request.cards.emplace_back(dasmig::card_kind::prepaid);
            else if (part == "any") request.cards.emplace_back(std::nullopt);
            else throw usage_error("--cards must list debit, credit, prepaid or any, not '" + part + "'");
        }
    if (const auto source = get(args, "card-source"))
    {
        if (*source == "network_range") request.card_numbers = dasmig::card_source::network_range;
        else if (*source == "test_number") request.card_numbers = dasmig::card_source::test_number;
        else if (*source == "wrong_check") request.card_numbers = dasmig::card_source::wrong_check;
        else throw usage_error("--card-source must be network_range, test_number or wrong_check");
    }
    if (const auto network = get(args, "network")) request.network = *network;
    request.safe = args.flags.contains("safe");
    request.weighted = !args.flags.contains("uniform");
    return request;
}

// The value whose name is `text`, from a list of names in the enum's order.
template <typename E, std::size_t N>
E named_value(const std::string& text, const std::array<std::string_view, N>& names, const std::string& option)
{
    for (std::size_t i = 0; i < N; ++i)
        if (names[i] == text) return static_cast<E>(i);
    std::string list;
    for (std::size_t i = 0; i < N; ++i) list += (i == 0 ? "" : i + 1 == N ? " or " : ", ") + std::string{names[i]};
    throw usage_error(option + " must be " + list);
}

// --area, --type, --tenure and --uniform as a request for a home; the
// property kind adds the household, the age and the income.
dasmig::property_request property_options(const arguments& args)
{
    static constexpr std::array<std::string_view, 3> areas{"city", "town", "rural"};
    static constexpr std::array<std::string_view, 6> types{"detached",   "semi_detached", "flat_small",
                                                           "flat_large", "informal",      "other"};
    static constexpr std::array<std::string_view, 4> tenures{"owner_outright", "owner_mortgage", "tenant_market",
                                                             "tenant_reduced"};
    dasmig::property_request request;
    if (const auto area = get(args, "area")) request.area = named_value<dasmig::settlement>(*area, areas, "--area");
    if (const auto type = get(args, "type")) request.type = named_value<dasmig::dwelling_type>(*type, types, "--type");
    if (const auto tenure = get(args, "tenure"))
        request.holding = named_value<dasmig::tenure>(*tenure, tenures, "--tenure");
    request.weighted = !args.flags.contains("uniform");
    if (args.kind != "property") return request;

    if (const auto country = get(args, "country")) request.country = upper(*country);
    if (const auto size = get(args, "household-size"))
    {
        const auto n = parse_count(*size, "--household-size");
        if (n < 1 || n > 30) throw usage_error("--household-size must be 1 to 30");
        request.household_size = static_cast<std::uint8_t>(n);
    }
    if (const auto type = get(args, "household-type"))
    {
        if (!std::ranges::contains(dasmig::property_household_types, std::string_view{*type}))
            throw usage_error("--household-type must be one_person, couple, nuclear, single_parent, with_parents, "
                              "multi_generation or other");
        request.household_type = *type;
    }
    if (const auto children = get(args, "children"))
    {
        const auto n = parse_count(*children, "--children");
        if (n > 29) throw usage_error("--children must be 0 to 29");
        request.children = static_cast<std::uint8_t>(n);
    }
    if (const auto age = get(args, "age"))
    {
        const auto n = parse_single_age(*age);
        if (n < 16 || n > 110) throw usage_error("--age must be 16 to 110 for property (the reference person's)");
        request.anchor_age = static_cast<std::uint8_t>(n);
    }
    if (const auto date = get(args, "reference-date")) request.reference_date = parse_date(*date);
    if (const auto income = get(args, "income"))
    {
        const auto amount = parse_decimal(*income, "--income");
        if (!(amount >= 0.0) || amount > 1e12) throw usage_error("--income must be an amount from 0 to 10^12");
        // The currency the country's incomes are in (occupation-generator's,
        // which property-generator's agrees with for every country they share).
        const auto currency = dasmig::ocg::instance().income_currency(*request.country);
        if (!currency)
            throw std::invalid_argument("no income currency is known for " + *request.country +
                                        "; leave --income out for a home without a cost");
        request.household_income = core::money::from_major(amount, currency->first, currency->second);
    }
    return request;
}

// A person drawn for --with-name.
struct drawn_person
{
    std::string country;
    std::chrono::year_month_day birth_date;
    dasmig::sex bio_sex;
    std::string full_name;
    std::string given;
    std::string surname;        // the first family name, or the patronymic where there is none
    std::string second_surname; // the second of a double surname; empty for none
};

std::optional<std::string> given_text(std::string text)
{
    if (text.empty()) return std::nullopt;
    return text;
}

std::string name_part(const dasmig::name& n, dasmig::name_role role)
{
    std::string out;
    for (std::size_t i = 0; i < n.parts().size(); ++i)
    {
        if (n.roles()[i] != role) continue;
        if (!out.empty()) out.push_back(' ');
        out += core::to_utf8(n.parts()[i]);
    }
    return out;
}

// Draws the birth and the name. With no country given, `draw_country` draws one
// from a seed, and a country with no names or births is drawn again.
template <typename DrawCountry>
drawn_person draw_person(const std::optional<std::string>& country, const dasmig::birth_request& ask, std::uint64_t seed,
                         const DrawCountry& draw_country)
{
    const auto country_seed = core::rng{seed}.split("country").seed();
    for (std::uint64_t attempt = 0; attempt < 256; ++attempt)
    {
        std::string code = country ? upper(*country) : draw_country(core::hash_combine(country_seed, attempt));
        const auto culture = dasmig::ng::to_culture(core::from_utf8(lower(code)));
        if (culture == dasmig::culture::any)
        {
            if (country) throw std::invalid_argument("--with-name: name-generator has no names for country " + code);
            continue;
        }
        try
        {
            auto request = ask;
            request.country_code = code;
            const auto birth = dasmig::bthg::instance().get_birth(request, core::rng{seed}.split("birth").seed());
            const auto n = dasmig::ng::instance().get_full_name(
                {.gender = birth.bio_sex == dasmig::sex::female ? dasmig::gender::f : dasmig::gender::m, .culture = culture},
                core::rng{seed}.split("name").seed());
            return drawn_person{.country = code,
                                .birth_date = std::chrono::year{birth.year} / std::chrono::month{birth.month} /
                                              std::chrono::day{birth.day},
                                .bio_sex = birth.bio_sex,
                                .full_name = core::to_utf8(static_cast<std::wstring>(n)),
                                .given = core::to_utf8(n.given()),
                                .surname = n.family().empty() ? core::to_utf8(n.patronymic())
                                                              : name_part(n, dasmig::name_role::family),
                                .second_surname = name_part(n, dasmig::name_role::family2)};
        }
        catch (const std::invalid_argument&)
        {
            if (country) throw;
        }
    }
    throw std::runtime_error("--with-name: no country drawn has names and births");
}

// The JSON of a result with the person's name as its first member.
std::string with_name_member(const std::string& name, const std::string& json)
{
    core::json_writer w;
    w.begin_object().field("name", name).end_object();
    std::string out = w.str();
    out.back() = ',';
    out.append(json, 1, std::string::npos);
    return out;
}

// The key for --unique of an item without the keyed value: one no other item
// has, so that it is never taken for a duplicate.
std::string unique_key_or_seed(const std::string& key, std::uint64_t seed)
{
    return key.empty() ? "no key " + hex16(seed) : key;
}

// validate SCHEME TEXT: id-generator's schemes, finance-generator's, and "card".
int run_validate(const arguments& args, const output_format& format, std::ostream& out)
{
    const auto& scheme = args.operands[0];
    const auto& text = args.operands[1];
    const auto date = get(args, "reference-date");
    core::json_writer w;
    w.begin_object().field("scheme", scheme).field("text", text);
    bool valid = false;
    if (scheme == "card")
    {
        if (date) throw usage_error("--reference-date applies only to id-generator's schemes");
        const auto& banks = dasmig::fng::instance();
        valid = banks.card_valid(text);
        w.field("valid", valid).key("networks").begin_array();
        if (valid)
            for (const auto& network : banks.card_networks(text)) w.value(network);
        w.end_array();
    }
    else
    {
        bool known = true;
        try
        {
            std::optional<std::chrono::year_month_day> reference;
            if (date) reference = parse_date(*date);
            valid = dasmig::idg::instance().is_valid(scheme, text, reference);
        }
        catch (const std::invalid_argument&)
        {
            known = false;
        }
        if (!known)
        {
            if (date) throw usage_error("--reference-date applies only to id-generator's schemes");
            try
            {
                valid = dasmig::fng::instance().is_valid(scheme, text);
            }
            catch (const std::invalid_argument&)
            {
                throw std::runtime_error("unknown scheme '" + scheme + "': not a scheme of id-generator or finance-generator, or \"card\"");
            }
        }
        w.field("valid", valid);
    }
    w.end_object();
    emit_list(format, out, {w.str()});
    return valid ? 0 : 3;
}

void load_full_tier(const arguments& args)
{
    if (args.tier != "full") return;
    if (!args.data_dir.empty()) std::filesystem::current_path(args.data_dir);
    const auto check = [](bool loaded, const char* library)
    {
        if (!loaded) throw std::runtime_error(std::string{"the full tier of "} + library +
                                              " was not found; give --data-dir the folder that holds the "
                                              "libraries' checkouts, e.g. <DIR>/" + library + "/resources");
    };
    const bool all = args.kind == "persona";
    const bool is_user = args.kind == "user";
    const bool is_family = args.kind == "family";
    const auto wants = [&](const char* kind) { return all || is_user || args.kind == kind; };
    const bool with_name = args.flags.contains("with-name");
    if (wants("name") || wants("surname") || is_family || all || with_name)
        check(dasmig::ng::instance().load(dasmig::dataset::full), "name-generator");
    const bool weather_city = args.kind == "weather" && args.flags.contains("city");
    if (wants("city") || all || weather_city) check(dasmig::cg::instance().load(dasmig::dataset::full), "city-generator");
    if (args.kind == "weather") check(dasmig::wtg::instance().load(dasmig::dataset::full), "weather-generator");
    const bool country_data = args.kind == "locale" || args.kind == "timezone" || args.kind == "holidays";
    if (wants("country") || all || country_data)
        check(dasmig::cntg::instance().load(dasmig::dataset::full), "country-generator");
    if ((country_data || is_user) && !dasmig::cntg::instance().has_locale_data())
        throw std::runtime_error("the full tier of country-generator has no locale tables; check out "
                                 "country-generator 3.3.0 or later");
    if (wants("birth") || is_family || all || with_name) check(dasmig::bthg::instance().load(dasmig::dataset::full), "birth-generator");
    if (is_family || (all && (args.flags.contains("relations") || args.flags.contains("property"))))
        check(dasmig::relg::instance().load(dasmig::dataset::full), "relationship-generator");
    if (wants("biodata") || all) check(dasmig::bdg::instance().load(dasmig::dataset::full), "biodata-generator");
    if (wants("interest") || all) check(dasmig::intg::instance().load(dasmig::dataset::full), "interest-generator");
    if (wants("occupation") || all) check(dasmig::ocg::instance().load(dasmig::dataset::full), "occupation-generator");
    if (is_user)
    {
        check(dasmig::adg::instance().load(dasmig::dataset::full), "address-generator");
        check(dasmig::phg::instance().load(dasmig::dataset::full), "phone-generator");
        check(dasmig::emg::instance().load(dasmig::dataset::full), "email-generator");
        check(dasmig::usg::instance().load(dasmig::dataset::full), "user-generator");
    }
    if (args.kind == "device" || (is_user && args.flags.contains("device")))
        check(dasmig::dvg::instance().load(dasmig::dataset::full), "device-generator");
    if (wants("nickname")) check(dasmig::nng::instance().load(dasmig::dataset::full), "nickname-generator");
    if (args.kind == "identity" || args.kind == "validate" || args.flags.contains("identity"))
        check(dasmig::idg::instance().load(dasmig::dataset::full), "id-generator");
    if (args.kind == "finance" || args.kind == "validate" || args.flags.contains("finance"))
        check(dasmig::fng::instance().load(dasmig::dataset::full), "finance-generator");
    if (args.kind == "property" || args.flags.contains("property"))
        check(dasmig::pty::instance().load(dasmig::dataset::full), "property-generator");
    if (args.kind == "property" && args.request.contains("income"))
        check(dasmig::ocg::instance().load(dasmig::dataset::full), "occupation-generator");
    if (all || wants("city") || weather_city)
    {
        const auto admin1 = std::filesystem::path{"city-generator"} / "resources" / "admin1CodesASCII.txt";
        auto& cities = dasmig::cg::instance();
        if (std::filesystem::exists(admin1) && !cities.has_admin1()) cities.load_admin1(admin1);
    }
}

// The months of 1991-2020 for --historical, from --data-dir (or the current
// folder) as the full tier is.
void load_weather_history(const std::filesystem::path& base)
{
    const auto file = base / "weather-generator" / "resources" / "history" / "history.tsv";
    auto& gen = dasmig::wtg::instance();
    if (std::filesystem::is_regular_file(file)) gen.load_history(file);
    else if (!gen.load_history())
        throw std::runtime_error("--historical needs " + file.generic_string() +
                                 "; give --data-dir the folder that holds the weather-generator checkout");
}

int run(const arguments& args)
{
    check_request_options(args);
    const auto data_base = args.data_dir.empty() ? std::filesystem::current_path()
                                                 : std::filesystem::absolute(args.data_dir);
    load_full_tier(args);
    if (args.kind == "weather" && args.flags.contains("historical")) load_weather_history(data_base);

    const bool lookup = args.kind == "holidays" || args.kind == "validate";
    const std::uint64_t seed = args.seed ? *args.seed : lookup ? 0 : core::rng::entropy_seed();
    if (!args.seed && !lookup) std::fprintf(stderr, "seed: %s\n", hex16(seed).c_str());
    core::batch_options options{.count = args.count, .first = args.first, .threads = args.threads};
    if (get(args, "unique"))
    {
        options.unique = core::unique_rule::by<keyed_json>([](const keyed_json& item) { return item.key; });
    }

    std::ofstream file;
    if (!args.out.empty())
    {
        file.open(args.out, std::ios::binary);
        if (!file) throw std::runtime_error("cannot write " + args.out);
    }
    std::ostream& out = args.out.empty() ? std::cout : file;
    output_format format{.name = args.format, .nullable = {}};
    if (args.format == "csv" && (args.kind == "occupation" || args.kind == "persona"))
        format.nullable = occupation_nullable(args.kind == "persona" ? "occupation." : "");
    if (args.format == "csv" && (args.kind == "property" || args.flags.contains("property")))
    {
        const auto money = std::vector<std::string>{"amount", "currency"};
        const auto object = [&](const std::string& column)
        {
            nullable_object out{.column = column, .columns = {}};
            for (const auto& part : money) out.columns.push_back(column + "." + part);
            return out;
        };
        format.nullable.push_back(object(args.kind == "persona" ? "home.monthly_cost" : "monthly_cost"));
        if (args.kind == "persona") format.nullable.push_back(object("household_income"));
    }
    const bool uniform = args.flags.contains("uniform");
    const auto country = get(args, "country");
    const auto sex_text = get(args, "sex");
    const auto age_text = get(args, "age");
    const auto date_text = get(args, "reference-date");
    const auto traits_text = get(args, "traits");

    if (args.kind == "validate")
    {
        const int status = run_validate(args, format, out);
        out.flush();
        if (!out) throw std::runtime_error("writing the output failed");
        return status;
    }

    if (args.kind == "persona")
    {
        dasmig::persona_request request;
        if (country) request.country = upper(*country);
        if (sex_text) request.person_sex = parse_sex(*sex_text);
        if (age_text) request.ages = parse_ages(*age_text);
        if (date_text) request.reference_date = parse_date(*date_text);
        if (traits_text) request.traits = static_cast<std::size_t>(parse_count(*traits_text, "--traits"));
        if (args.flags.contains("relations"))
        {
            dasmig::family_request family;
            if (const auto up = get(args, "up")) family.up = static_cast<int>(parse_count(*up, "--up"));
            if (const auto down = get(args, "down")) family.down = static_cast<int>(parse_count(*down, "--down"));
            family.stepparents = args.flags.contains("stepparents");
            family.home = args.flags.contains("home");
            request.relations = family;
        }
        if (args.flags.contains("identity")) request.identity = identity_options(args);
        if (args.flags.contains("finance")) request.finance = finance_options(args);
        if (args.flags.contains("property")) request.home = property_options(args);
        const auto& gen = dasmig::psg::instance();
        emit(format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_persona(request, s).to_json(); });
    }
    else if (args.kind == "name" || args.kind == "surname")
    {
        dasmig::name_request request;
        if (country)
        {
            request.culture = dasmig::ng::to_culture(core::from_utf8(lower(*country)));
            if (request.culture == dasmig::culture::any)
                throw std::invalid_argument("name-generator has no names for country " + upper(*country));
        }
        if (const auto g = get(args, "gender"))
            request.gender = dasmig::ng::to_gender(core::from_utf8(*g));
        const auto& gen = dasmig::ng::instance();
        if (args.kind == "name")
            emit(format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_name(request, s).to_json(); });
        else
            emit(format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_surname(request, s).to_json(); });
    }
    else if (args.kind == "nickname")
    {
        dasmig::nickname_request request;
        if (const auto n = get(args, "name")) request.name = core::from_utf8(*n);
        const auto& gen = dasmig::nng::instance();
        emit(format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_nickname(request, s).to_json(); });
    }
    else if (args.kind == "city")
    {
        dasmig::city_request request;
        if (country) request.country = upper(*country);
        request.weighted = !uniform;
        const auto& gen = dasmig::cg::instance();
        emit(format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_city(request, s).to_json(); });
    }
    else if (args.kind == "country")
    {
        dasmig::country_request request;
        if (const auto r = get(args, "region")) request.region = *r;
        request.weighted = !uniform;
        const auto& gen = dasmig::cntg::instance();
        emit(format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_country(request, s).to_json(); });
    }
    else if (args.kind == "birth")
    {
        dasmig::birth_request request;
        if (country) request.country_code = upper(*country);
        if (sex_text) request.bio_sex = parse_sex(*sex_text);
        if (age_text) request.ages = parse_ages(*age_text);
        if (date_text) request.reference_date = parse_date(*date_text);
        request.weighted = !uniform;
        const auto& gen = dasmig::bthg::instance();
        emit(format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_birth(request, s).to_json(); });
    }
    else if (args.kind == "biodata")
    {
        const std::string code = country ? upper(*country) : std::string{};
        dasmig::biodata_request request;
        request.country = code;
        if (sex_text) request.bio_sex = parse_sex(*sex_text);
        if (age_text) request.age = parse_single_age(*age_text);
        const auto& gen = dasmig::bdg::instance();
        emit(format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_biodata(request, s).to_json(); });
    }
    else if (args.kind == "trait")
    {
        dasmig::trait_request request;
        if (traits_text) request.count = static_cast<std::size_t>(parse_count(*traits_text, "--traits"));
        const auto& gen = dasmig::trtg::instance();
        emit(format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_personality(request, s).to_json(); });
    }
    else if (args.kind == "interest")
    {
        const std::string code = country ? upper(*country) : std::string{};
        dasmig::interest_request request;
        request.country = code;
        if (sex_text) request.person_sex = parse_sex(*sex_text);
        if (age_text) request.age = static_cast<std::uint8_t>(parse_single_age(*age_text));
        const auto& gen = dasmig::intg::instance();
        emit(format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_interest(request, s).to_json(); });
    }
    else if (args.kind == "user")
    {
        dasmig::user_request request;
        if (country) request.country = upper(*country);
        if (sex_text) request.person_sex = parse_sex(*sex_text);
        if (age_text) request.ages = parse_ages(*age_text);
        if (date_text) request.reference_date = parse_date(*date_text);
        request.safe = args.flags.contains("safe");
        request.device = args.flags.contains("device");
        if (args.flags.contains("identity")) request.identity = identity_options(args);
        if (args.flags.contains("finance")) request.finance = finance_options(args);
        const bool by_username = get(args, "unique") == std::optional<std::string>{"username"};
        const auto& gen = dasmig::usg::instance();
        emit_keyed(format, out, options, seed,
                   [&](std::uint64_t s)
                   {
                       const auto user = gen.get_user(request, s);
                       return keyed_json{user.to_json(), by_username ? user.account.username : user.mail.address()};
                   });
    }
    else if (args.kind == "device")
    {
        dasmig::device_request request;
        if (country) request.country = upper(*country);
        if (date_text) request.reference_date = parse_date(*date_text);
        if (const auto p = get(args, "platform")) request.platform = *p;
        if (const auto b = get(args, "browser")) request.browser = *b;
        if (const auto l = get(args, "locale")) request.locale = *l;
        if (const auto ip = get(args, "ip"))
            request.ip = *ip == "v4" ? dasmig::ip_version::v4 : *ip == "v6" ? dasmig::ip_version::v6 : dasmig::ip_version::any;
        request.safe = args.flags.contains("safe");
        request.weighted = !uniform;
        const auto& gen = dasmig::dvg::instance();
        emit_keyed(format, out, options, seed,
                   [&](std::uint64_t s)
                   {
                       const auto device = gen.get_device(request, s);
                       return keyed_json{device.to_json(), device.ip};
                   });
    }
    else if (args.kind == "family")
    {
        dasmig::family_request request;
        if (country) request.anchor.country_code = upper(*country);
        if (sex_text) request.anchor.bio_sex = parse_sex(*sex_text);
        if (age_text) request.anchor.age = static_cast<int>(parse_single_age(*age_text));
        if (date_text) request.reference_date = parse_date(*date_text);
        if (const auto up = get(args, "up")) request.up = static_cast<int>(parse_count(*up, "--up"));
        if (const auto down = get(args, "down")) request.down = static_cast<int>(parse_count(*down, "--down"));
        request.weighted = !uniform;
        request.stepparents = args.flags.contains("stepparents");
        request.home = args.flags.contains("home");
        const auto& gen = dasmig::relg::instance();
        emit(format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_family(request, s).to_json(); });
    }
    else if (args.kind == "weather")
    {
        dasmig::weather_request request;
        if (const auto lat = get(args, "lat")) request.latitude = parse_decimal(*lat, "--lat");
        if (const auto lon = get(args, "lon")) request.longitude = parse_decimal(*lon, "--lon");
        if (const auto start = get(args, "start")) request.date = parse_date(*start, "--start");
        if (const auto e = get(args, "elevation")) request.elevation = parse_decimal(*e, "--elevation");
        if (const auto w = get(args, "warming")) request.warming = parse_decimal(*w, "--warming");
        if (get(args, "units") == std::optional<std::string>{"imperial"}) request.units = dasmig::unit_system::imperial;
        request.hourly = args.flags.contains("hourly");
        request.historical = args.flags.contains("historical");
        request.spatial = !args.flags.contains("no-spatial");
        int days = 1;
        if (const auto d = get(args, "days"))
        {
            const auto n = parse_count(*d, "--days");
            if (n < 1 || n > static_cast<std::uint64_t>(dasmig::wtg::max_series_days)) throw usage_error("--days must be 1 to 36600");
            days = static_cast<int>(n);
        }
        const bool at_city = args.flags.contains("city");
        dasmig::city_request city_request;
        if (country) city_request.country = upper(*country);
        const auto& gen = dasmig::wtg::instance();
        const auto& cities = dasmig::cg::instance();
        emit_rows(format, out, options, seed,
                  [&](std::uint64_t s)
                  {
                      auto r = request;
                      std::string city_json;
                      if (at_city)
                      {
                          // A city whose weather the data covers (a remote
                          // island can be over 100 km from the land mask).
                          const core::rng city_stream = core::rng{s}.split("city");
                          for (std::uint64_t attempt = 0;; ++attempt)
                          {
                              if (attempt == 100) throw std::runtime_error("no city with weather data was drawn");
                              const auto c = cities.get_city(city_request, city_stream.split(attempt).seed());
                              try
                              {
                                  (void)gen.climate_at(c.latitude, c.longitude);
                              }
                              catch (const std::invalid_argument&)
                              {
                                  continue;
                              }
                              r.latitude = c.latitude;
                              r.longitude = c.longitude;
                              core::json_writer w;
                              w.begin_object().field("name", c.name).field("country_code", c.country_code).end_object();
                              city_json = "{\"city\":" + w.str() + ",";
                              break;
                          }
                      }
                      std::vector<std::string> rows;
                      for (const auto& day : gen.get_weather_series(r, days, s))
                      {
                          auto json = day.to_json();
                          if (!city_json.empty()) json = city_json + json.substr(1);
                          rows.push_back(std::move(json));
                      }
                      return rows;
                  });
    }
    else if (args.kind == "locale")
    {
        dasmig::locale_request request;
        if (country) request.country = upper(*country);
        if (const auto l = get(args, "language")) request.language = *l;
        request.weighted = !uniform;
        const auto& gen = dasmig::cntg::instance();
        emit(format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_locale(request, s).to_json(); });
    }
    else if (args.kind == "timezone")
    {
        dasmig::time_zone_request request;
        if (country) request.country = upper(*country);
        if (date_text) request.reference_date = parse_date(*date_text);
        request.weighted = !uniform;
        const auto& gen = dasmig::cntg::instance();
        emit(format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_time_zone(request, s).to_json(); });
    }
    else if (args.kind == "identity")
    {
        auto request = identity_options(args);
        if (country) request.country = upper(*country);
        if (sex_text) request.sex = parse_sex(*sex_text);
        if (date_text) request.reference_date = parse_date(*date_text);
        dasmig::birth_request ask;
        if (sex_text) ask.bio_sex = parse_sex(*sex_text);
        if (age_text) ask.ages = parse_ages(*age_text);
        ask.reference_date = request.reference_date;
        const bool with_name = args.flags.contains("with-name");
        const auto& gen = dasmig::idg::instance();
        emit_keyed(format, out, options, seed,
                   [&](std::uint64_t s)
                   {
                       auto own = request;
                       std::string full_name;
                       if (with_name)
                       {
                           const auto person = draw_person(country, ask, s, [&](std::uint64_t c)
                           {
                               dasmig::identity_request draw;
                               draw.weighted = request.weighted;
                               return gen.get_identity(draw, c).country_code;
                           });
                           own.country = person.country;
                           own.birth_date = person.birth_date;
                           own.sex = person.bio_sex;
                           own.given_names = given_text(person.given);
                           own.surname = given_text(person.surname);
                           own.second_surname = given_text(person.second_surname);
                           full_name = person.full_name;
                       }
                       const auto item = gen.get_identity(own, s);
                       const auto json = item.to_json();
                       return keyed_json{with_name ? with_name_member(full_name, json) : json,
                                         unique_key_or_seed(item.numbers.empty() ? std::string{} : item.numbers.front().value, s)};
                   });
    }
    else if (args.kind == "finance")
    {
        auto request = finance_options(args);
        if (country) request.country = upper(*country);
        if (date_text) request.reference_date = parse_date(*date_text);
        dasmig::birth_request ask;
        if (sex_text) ask.bio_sex = parse_sex(*sex_text);
        ask.ages = age_text ? parse_ages(*age_text) : dasmig::age_range{.min = 18, .max = 80};
        ask.reference_date = request.reference_date;
        const bool with_name = args.flags.contains("with-name");
        const bool by_card = get(args, "unique") == std::optional<std::string>{"card"};
        const auto& gen = dasmig::fng::instance();
        emit_keyed(format, out, options, seed,
                   [&](std::uint64_t s)
                   {
                       auto own = request;
                       std::string full_name;
                       if (with_name)
                       {
                           const auto person = draw_person(country, ask, s, [&](std::uint64_t c)
                           {
                               dasmig::finance_request draw;
                               draw.accounts.clear();
                               draw.weighted = request.weighted;
                               return gen.get_finance(draw, c).country_code;
                           });
                           own.country = person.country;
                           own.birth_date = person.birth_date;
                           own.given_names = given_text(person.given);
                           own.surname = given_text(person.surname);
                           full_name = person.full_name;
                       }
                       const auto item = gen.get_finance(own, s);
                       const auto json = item.to_json();
                       std::string key;
                       if (by_card && !item.cards.empty()) key = item.cards.front().number;
                       if (!by_card && !item.accounts.empty())
                       {
                           const auto& account = item.accounts.front();
                           key = account.iban.empty() ? account.bank_code + "/" + account.branch_code + "/" + account.account_number : account.iban;
                       }
                       return keyed_json{with_name ? with_name_member(full_name, json) : json, unique_key_or_seed(key, s)};
                   });
    }
    else if (args.kind == "property")
    {
        const auto request = property_options(args);
        const auto& gen = dasmig::pty::instance();
        emit(format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_property(request, s).to_json(); });
    }
    else if (args.kind == "holidays")
    {
        dasmig::holiday_request request;
        request.country = upper(*country);
        const auto year = parse_count(*get(args, "year"), "--year");
        if (year < 1 || year > 9999) throw usage_error("--year must be a year such as 2025");
        request.year = static_cast<int>(year);
        request.optional = args.flags.contains("optional");
        request.observances = args.flags.contains("observances");
        std::vector<std::string> rows;
        for (const auto& day : dasmig::cntg::instance().holidays(request)) rows.push_back(day.to_json());
        emit_list(format, out, rows);
    }
    else // occupation
    {
        const std::string code = country ? upper(*country) : std::string{};
        dasmig::occupation_request request;
        request.country = code;
        if (sex_text) request.person_sex = parse_sex(*sex_text);
        if (age_text) request.age = static_cast<std::uint8_t>(parse_single_age(*age_text));
        const auto& gen = dasmig::ocg::instance();
        emit(format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_occupation(request, s).to_json(); });
    }

    out.flush();
    if (!out) throw std::runtime_error("writing the output failed");
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
#if defined(_WIN32)
    _setmode(_fileno(stdout), _O_BINARY); // keep "\n" as it is
#endif
    std::ios::sync_with_stdio(false);
    try
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view arg{argv[i]};
            if (arg == "--help" || arg == "-h")
            {
                std::cout << help_text;
                return 0;
            }
            if (arg == "--version")
            {
                std::cout << "dasmig-gen " DASMIG_GEN_VERSION " (dasmig-core " << core::version.major << '.'
                          << core::version.minor << '.' << core::version.patch << ")\n";
                return 0;
            }
        }
        return run(parse_arguments(argc, argv));
    }
    catch (const usage_error& error)
    {
        std::fprintf(stderr, "dasmig-gen: %s\nTry 'dasmig-gen --help'.\n", error.what());
        return 2;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "dasmig-gen: %s\n", error.what());
        return 1;
    }
}
