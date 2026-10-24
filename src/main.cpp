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
#include <dasmig/interestgen.hpp>
#include <dasmig/namegen.hpp>
#include <dasmig/nicknamegen.hpp>
#include <dasmig/occupationgen.hpp>
#include <dasmig/personagen.hpp>
#include <dasmig/relgen.hpp>
#include <dasmig/traitgen.hpp>
#include <dasmig/usergen.hpp>

#include <algorithm>
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
#define DASMIG_GEN_VERSION "3.3.2"
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
  occupation  employment, education and job
  user        a user record (name, birth, address, phone, email, account)
  family      the people around a person (parents, siblings, partners, children)

Batch options (all kinds):
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
  --country CC       ISO 3166-1 alpha-2 code        (persona, name, surname, city, birth,
                                                     biodata, interest, occupation, user, family)
  --sex male|female                                 (persona, birth, biodata, interest, occupation, user, family)
  --age N or MIN-MAX                                (persona, birth, biodata, interest, occupation, user;
                                                     biodata, interest, occupation and family take one age)
  --reference-date YYYY-MM-DD   the date ages are computed at (persona, birth, user, family)
  --traits N         number of personality traits   (persona, trait)
  --gender m|f|any   (name, surname)
  --name TEXT        full name to derive from       (nickname)
  --region NAME      UN geoscheme region            (country)
  --up N, --down N   generations above and below the person (default 1 each) (family; persona with --relations)
  --relations        also draw the persona's family (parents, siblings, partners, children) as "relatives" (persona)
  --stepparents      also a parent's new partner, as stepfather or stepmother, and their children (family; persona with --relations)
  --home             also who the person lives with, as "household" (family; persona with --relations)
  --uniform          uniform instead of population-weighted choice (city, country, birth, family)
  --safe             contacts that reach no one: reserved email domains, fictional phone ranges (user)
  --unique KEY       no two items share the key: email or username (user)

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

std::chrono::year_month_day parse_date(const std::string& text)
{
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') throw usage_error("--reference-date must be YYYY-MM-DD");
    const auto year = static_cast<int>(parse_count(text.substr(0, 4), "--reference-date"));
    const auto month = static_cast<unsigned>(parse_count(text.substr(5, 2), "--reference-date"));
    const auto day = static_cast<unsigned>(parse_count(text.substr(8, 2), "--reference-date"));
    const std::chrono::year_month_day date{std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day}};
    if (!date.ok() || year < 1900 || year > 2100) throw usage_error("--reference-date is not a calendar date from 1900 to 2100");
    return date;
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
};

const std::set<std::string> value_options{"country", "sex", "age", "reference-date", "traits", "gender",
                                          "name",    "region", "unique", "up",     "down"};
const std::set<std::string> kinds{"persona", "name",      "surname", "nickname", "city",     "country",
                                  "birth",   "biodata",   "trait",   "interest", "occupation", "user",   "family"};

arguments parse_arguments(int argc, char** argv)
{
    arguments args;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (!arg.starts_with("--"))
        {
            if (!args.kind.empty()) throw usage_error("unexpected argument '" + arg + "'");
            args.kind = arg;
            continue;
        }
        std::string name = arg.substr(2);
        std::optional<std::string> inline_value;
        if (const auto eq = name.find('='); eq != std::string::npos)
        {
            inline_value = name.substr(eq + 1);
            name.resize(eq);
        }
        if (name == "uniform" || name == "safe" || name == "relations" || name == "stepparents" || name == "home")
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
    {"persona", {"country", "sex", "age", "reference-date", "traits", "uniform", "relations", "up", "down", "stepparents", "home"}},
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
    {"user", {"country", "sex", "age", "reference-date", "safe", "unique"}},
    {"family", {"country", "sex", "age", "reference-date", "uniform", "up", "down", "stepparents", "home"}}};

void check_request_options(const arguments& args)
{
    const auto& ok = accepted.at(args.kind);
    for (const auto& [name, _] : args.request)
        if (!ok.contains(name)) throw usage_error("--" + name + " does not apply to " + args.kind);
    if (const auto unique = args.request.find("unique");
        unique != args.request.end() && unique->second != "email" && unique->second != "username")
        throw usage_error("--unique must be email or username");
    for (const auto& name : args.flags)
        if (!ok.contains(name)) throw usage_error("--" + name + " does not apply to " + args.kind);
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
void emit(const std::string& format, std::ostream& out, const core::batch_options& options,
          std::uint64_t seed, const Make& make)
{
    if (format == "jsonl")
    {
        core::jsonl_writer writer{out};
        core::for_each_batch(options, seed, make,
                             [&writer](std::uint64_t, const std::string& json) { writer.write_line(json); });
    }
    else if (format == "csv")
    {
        core::csv_writer writer{out};
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
void emit_keyed(const std::string& format, std::ostream& out, const core::batch_options& options,
                std::uint64_t seed, const Make& make)
{
    if (format == "jsonl")
    {
        core::jsonl_writer writer{out};
        core::for_each_batch(options, seed, make,
                             [&writer](std::uint64_t, const keyed_json& item) { writer.write_line(item.json); });
    }
    else if (format == "csv")
    {
        core::csv_writer writer{out};
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
    if (wants("name") || wants("surname") || is_family || all)
        check(dasmig::ng::instance().load(dasmig::dataset::full), "name-generator");
    if (wants("city") || all) check(dasmig::cg::instance().load(dasmig::dataset::full), "city-generator");
    if (wants("country") || all) check(dasmig::cntg::instance().load(dasmig::dataset::full), "country-generator");
    if (wants("birth") || is_family || all) check(dasmig::bthg::instance().load(dasmig::dataset::full), "birth-generator");
    if (is_family || (all && args.flags.contains("relations"))) check(dasmig::relg::instance().load(dasmig::dataset::full), "relationship-generator");
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
    if (wants("nickname")) check(dasmig::nng::instance().load(dasmig::dataset::full), "nickname-generator");
    if (all || wants("city"))
    {
        const auto admin1 = std::filesystem::path{"city-generator"} / "resources" / "admin1CodesASCII.txt";
        auto& cities = dasmig::cg::instance();
        if (std::filesystem::exists(admin1) && !cities.has_admin1()) cities.load_admin1(admin1);
    }
}

int run(const arguments& args)
{
    check_request_options(args);
    load_full_tier(args);

    const std::uint64_t seed = args.seed ? *args.seed : core::rng::entropy_seed();
    if (!args.seed) std::fprintf(stderr, "seed: %s\n", hex16(seed).c_str());
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
    const bool uniform = args.flags.contains("uniform");
    const auto country = get(args, "country");
    const auto sex_text = get(args, "sex");
    const auto age_text = get(args, "age");
    const auto date_text = get(args, "reference-date");
    const auto traits_text = get(args, "traits");

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
        const auto& gen = dasmig::psg::instance();
        emit(args.format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_persona(request, s).to_json(); });
    }
    else if (args.kind == "name" || args.kind == "surname")
    {
        dasmig::name_request request;
        if (country) request.culture = dasmig::ng::to_culture(core::from_utf8(upper(*country)));
        if (const auto g = get(args, "gender"))
            request.gender = dasmig::ng::to_gender(core::from_utf8(*g));
        const auto& gen = dasmig::ng::instance();
        if (args.kind == "name")
            emit(args.format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_name(request, s).to_json(); });
        else
            emit(args.format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_surname(request, s).to_json(); });
    }
    else if (args.kind == "nickname")
    {
        dasmig::nickname_request request;
        if (const auto n = get(args, "name")) request.name = core::from_utf8(*n);
        const auto& gen = dasmig::nng::instance();
        emit(args.format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_nickname(request, s).to_json(); });
    }
    else if (args.kind == "city")
    {
        dasmig::city_request request;
        if (country) request.country = upper(*country);
        request.weighted = !uniform;
        const auto& gen = dasmig::cg::instance();
        emit(args.format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_city(request, s).to_json(); });
    }
    else if (args.kind == "country")
    {
        dasmig::country_request request;
        if (const auto r = get(args, "region")) request.region = *r;
        request.weighted = !uniform;
        const auto& gen = dasmig::cntg::instance();
        emit(args.format, out, options, seed,
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
        emit(args.format, out, options, seed,
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
        emit(args.format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_biodata(request, s).to_json(); });
    }
    else if (args.kind == "trait")
    {
        dasmig::trait_request request;
        if (traits_text) request.count = static_cast<std::size_t>(parse_count(*traits_text, "--traits"));
        const auto& gen = dasmig::trtg::instance();
        emit(args.format, out, options, seed,
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
        emit(args.format, out, options, seed,
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
        const bool by_username = get(args, "unique") == std::optional<std::string>{"username"};
        const auto& gen = dasmig::usg::instance();
        emit_keyed(args.format, out, options, seed,
                   [&](std::uint64_t s)
                   {
                       const auto user = gen.get_user(request, s);
                       return keyed_json{user.to_json(), by_username ? user.account.username : user.mail.address()};
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
        emit(args.format, out, options, seed,
             [&](std::uint64_t s) { return gen.get_family(request, s).to_json(); });
    }
    else // occupation
    {
        const std::string code = country ? upper(*country) : std::string{};
        dasmig::occupation_request request;
        request.country = code;
        if (sex_text) request.person_sex = parse_sex(*sex_text);
        if (age_text) request.age = static_cast<std::uint8_t>(parse_single_age(*age_text));
        const auto& gen = dasmig::ocg::instance();
        emit(args.format, out, options, seed,
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
