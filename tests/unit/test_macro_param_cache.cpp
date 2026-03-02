// SPDX-License-Identifier: GPL-3.0-or-later

#include "macro_param_cache.h"

#include "../catch_amalgamated.hpp"

using helix::CachedMacroInfo;
using helix::MacroParamCache;
using helix::MacroParamKnowledge;

// ============================================================================
// MacroParamCache Tests
// ============================================================================

TEST_CASE("MacroParamCache populate categorizes params correctly", "[macro_param_cache]") {
    auto& cache = MacroParamCache::instance();
    cache.clear();

    nlohmann::json config;
    config["gcode_macro clean_nozzle"] = {
        {"gcode", "{% set TEMP = params.TEMP|default(240)|int %}\nG1 E10"}};
    config["gcode_macro cancel_print"] = {
        {"gcode", "TURN_OFF_HEATERS\nBASE_CANCEL"}};
    config["gcode_macro pause"] = {
        {"gcode", "SAVE_GCODE_STATE\nBASE_PAUSE"}};

    std::unordered_set<std::string> known_macros = {
        "CLEAN_NOZZLE", "CANCEL_PRINT", "PAUSE", "GET_VARIABLE"};

    cache.populate_from_configfile(config, known_macros);

    SECTION("macro with params is KNOWN_PARAMS") {
        auto info = cache.get("CLEAN_NOZZLE");
        REQUIRE(info.knowledge == MacroParamKnowledge::KNOWN_PARAMS);
        REQUIRE(info.params.size() == 1);
        REQUIRE(info.params[0].name == "TEMP");
        REQUIRE(info.params[0].default_value == "240");
    }

    SECTION("macro without params is KNOWN_NO_PARAMS") {
        auto info = cache.get("CANCEL_PRINT");
        REQUIRE(info.knowledge == MacroParamKnowledge::KNOWN_NO_PARAMS);
        REQUIRE(info.params.empty());
    }

    SECTION("macro without params (pause) is KNOWN_NO_PARAMS") {
        auto info = cache.get("PAUSE");
        REQUIRE(info.knowledge == MacroParamKnowledge::KNOWN_NO_PARAMS);
        REQUIRE(info.params.empty());
    }

    SECTION("known macro not in configfile is UNKNOWN") {
        auto info = cache.get("GET_VARIABLE");
        REQUIRE(info.knowledge == MacroParamKnowledge::UNKNOWN);
        REQUIRE(info.params.empty());
    }
}

TEST_CASE("MacroParamCache get returns UNKNOWN for totally unknown macro", "[macro_param_cache]") {
    auto& cache = MacroParamCache::instance();
    cache.clear();

    auto info = cache.get("NONEXISTENT_MACRO");
    REQUIRE(info.knowledge == MacroParamKnowledge::UNKNOWN);
    REQUIRE(info.params.empty());
}

TEST_CASE("MacroParamCache clear resets state", "[macro_param_cache]") {
    auto& cache = MacroParamCache::instance();
    cache.clear();

    nlohmann::json config;
    config["gcode_macro test_macro"] = {
        {"gcode", "{% set X = params.X|default(0) %}"}};

    cache.populate_from_configfile(config, {});

    REQUIRE(cache.get("TEST_MACRO").knowledge == MacroParamKnowledge::KNOWN_PARAMS);

    cache.clear();

    REQUIRE(cache.get("TEST_MACRO").knowledge == MacroParamKnowledge::UNKNOWN);
}

TEST_CASE("MacroParamCache case-insensitive lookup", "[macro_param_cache]") {
    auto& cache = MacroParamCache::instance();
    cache.clear();

    nlohmann::json config;
    config["gcode_macro print_start"] = {
        {"gcode", "{% set BED = params.BED_TEMP|default(60) %}"}};

    cache.populate_from_configfile(config, {});

    // All case variations should work
    REQUIRE(cache.get("PRINT_START").knowledge == MacroParamKnowledge::KNOWN_PARAMS);
    REQUIRE(cache.get("print_start").knowledge == MacroParamKnowledge::KNOWN_PARAMS);
    REQUIRE(cache.get("Print_Start").knowledge == MacroParamKnowledge::KNOWN_PARAMS);
}

TEST_CASE("MacroParamCache multiple params extracted", "[macro_param_cache]") {
    auto& cache = MacroParamCache::instance();
    cache.clear();

    nlohmann::json config;
    config["gcode_macro start_print"] = {
        {"gcode",
         "{% set BED_TEMP = params.BED_TEMP|default(60)|float %}\n"
         "{% set EXTRUDER_TEMP = params.EXTRUDER_TEMP|default(200)|float %}\n"
         "{% set CHAMBER_TEMP = params.CHAMBER_TEMP|default(0)|float %}\nG28"}};

    cache.populate_from_configfile(config, {});

    auto info = cache.get("START_PRINT");
    REQUIRE(info.knowledge == MacroParamKnowledge::KNOWN_PARAMS);
    REQUIRE(info.params.size() == 3);
}

TEST_CASE("MacroParamCache handles missing gcode field", "[macro_param_cache]") {
    auto& cache = MacroParamCache::instance();
    cache.clear();

    nlohmann::json config;
    config["gcode_macro no_gcode"] = {{"description", "test"}};

    cache.populate_from_configfile(config, {});

    auto info = cache.get("NO_GCODE");
    REQUIRE(info.knowledge == MacroParamKnowledge::KNOWN_NO_PARAMS);
}

// ============================================================================
// parse_raw_params Tests
// ============================================================================

TEST_CASE("parse_raw_params basic", "[macro_param_cache]") {
    auto result = MacroParamCache::parse_raw_params("TEMP=200 SPEED=50");
    REQUIRE(result.size() == 2);
    REQUIRE(result["TEMP"] == "200");
    REQUIRE(result["SPEED"] == "50");
}

TEST_CASE("parse_raw_params empty input", "[macro_param_cache]") {
    auto result = MacroParamCache::parse_raw_params("");
    REQUIRE(result.empty());
}

TEST_CASE("parse_raw_params whitespace only", "[macro_param_cache]") {
    auto result = MacroParamCache::parse_raw_params("   ");
    REQUIRE(result.empty());
}

TEST_CASE("parse_raw_params skips missing equals", "[macro_param_cache]") {
    auto result = MacroParamCache::parse_raw_params("JUSTVALUE TEMP=200");
    REQUIRE(result.size() == 1);
    REQUIRE(result["TEMP"] == "200");
}

TEST_CASE("parse_raw_params extra whitespace", "[macro_param_cache]") {
    auto result = MacroParamCache::parse_raw_params("  TEMP=200   SPEED=50  ");
    REQUIRE(result.size() == 2);
    REQUIRE(result["TEMP"] == "200");
    REQUIRE(result["SPEED"] == "50");
}

TEST_CASE("parse_raw_params uppercases keys", "[macro_param_cache]") {
    auto result = MacroParamCache::parse_raw_params("temp=200 speed=50");
    REQUIRE(result.size() == 2);
    REQUIRE(result["TEMP"] == "200");
    REQUIRE(result["SPEED"] == "50");
}

TEST_CASE("parse_raw_params preserves value case", "[macro_param_cache]") {
    auto result = MacroParamCache::parse_raw_params("NAME=MyVariable");
    REQUIRE(result.size() == 1);
    REQUIRE(result["NAME"] == "MyVariable");
}

TEST_CASE("parse_raw_params skips equals at start", "[macro_param_cache]") {
    auto result = MacroParamCache::parse_raw_params("=value TEMP=200");
    REQUIRE(result.size() == 1);
    REQUIRE(result["TEMP"] == "200");
}
