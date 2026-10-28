CXX      ?= clang++
# The libraries are sibling checkouts (or checked out under libs/ in CI); set
# LIBS to the folder that holds them. Their lite data is compiled in.
LIBS     ?= ..
LIB_DIRS := address biodata birth city country device email entity interest name nickname occupation persona phone relationship trait user weather
EMBED    := $(addprefix -DDASMIG_,ADDRESS_EMBED BIODATA_EMBED BIRTH_EMBED CITY_EMBED COUNTRY_EMBED DEVICE_EMBED EMAIL_EMBED INTEREST_EMBED NAME_EMBED NICKNAME_EMBED OCCUPATION_EMBED PHONE_EMBED RELATIONSHIP_EMBED TRAIT_EMBED USER_EMBED WEATHER_EMBED)
CXXFLAGS ?= -O2 -DNDEBUG
CXXFLAGS += -std=c++23 -Wall -Wextra -Wpedantic -Wno-c2y-extensions $(foreach d,$(LIB_DIRS),-I$(LIBS)/$(d)-generator) $(EMBED)
VERSION  ?= 3.5.0
BUILD    := build
PYTHON   ?= python3

BIN := $(BUILD)/dasmig-gen

.PHONY: all test clean

all: $(BIN)

$(BIN): src/main.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -DDASMIG_GEN_VERSION='"$(VERSION)"' -o $@ src/main.cpp $(LDFLAGS)

test: $(BIN)
	$(PYTHON) tests/test_cli.py $(BIN)

$(BUILD):
	@mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
