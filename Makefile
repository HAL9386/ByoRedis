# Makefile for ByoRedis (refactored layout)
# Usage examples:
#   make                # build all (server, client)
#   make -j             # parallel build
#   make run-server     # run built server
#   make DEBUG=1        # debug build (-O0 -g3)
#   make SAN=address    # enable sanitizer(s), e.g., address,undefined
#   make tests          # build all tests in test/

# Tools and flags (override from CLI if needed)
CXX ?= g++
CPPFLAGS ?=
CPPFLAGS += -Iinclude

# Base C++ flags
CXXFLAGS ?=
CXXFLAGS += -std=c++20 -Wall -Wextra -O2 -g

# Optional debug and sanitizer knobs
ifeq ($(DEBUG),1)
  CXXFLAGS := $(filter-out -O2,$(CXXFLAGS))
  CXXFLAGS += -O0 -g3 -fno-omit-frame-pointer
endif

ifneq ($(SAN),)
  CXXFLAGS += -fsanitize=$(SAN)
  LDFLAGS  += -fsanitize=$(SAN)
endif

# Output directories
BUILDDIR := build
BINDIR   := bin
TESTBINDIR := $(BINDIR)/tests

# Sources
SRCS_COMMON  := $(wildcard src/common/*.cc) $(wildcard src/proto/*.cc)
SRCS_SERVER  := $(wildcard src/server/*.cc) $(wildcard src/ds/*.cc)
SRCS_CLIENT  := $(wildcard src/client/*.cc)

# Objects (mirror directory structure under build/)
OBJS_COMMON := $(patsubst src/%.cc,$(BUILDDIR)/%.o,$(SRCS_COMMON))
OBJS_SERVER := $(patsubst src/%.cc,$(BUILDDIR)/%.o,$(SRCS_SERVER))
OBJS_CLIENT := $(patsubst src/%.cc,$(BUILDDIR)/%.o,$(SRCS_CLIENT))

# Dependencies
DEPS := $(OBJS_COMMON:.o=.d) $(OBJS_SERVER:.o=.d) $(OBJS_CLIENT:.o=.d)

# Tests (conventional handling)
TEST_SRCS := $(wildcard test/*.cc)
TEST_OBJS := $(patsubst test/%.cc,$(BUILDDIR)/test/%.o,$(TEST_SRCS))
TEST_DEPS := $(TEST_OBJS:.o=.d)
TEST_BINS := $(patsubst test/%.cc,$(TESTBINDIR)/%,$(TEST_SRCS))

# Default goal
.DEFAULT_GOAL := all

# Phony targets
.PHONY: all clean distclean run-server run-client help tests

all: ## Build all targets (server, client)
all: $(BINDIR)/server $(BINDIR)/client

# Link steps
$(BINDIR)/server: $(OBJS_SERVER) $(OBJS_COMMON) | $(BINDIR)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BINDIR)/client: $(OBJS_CLIENT) $(OBJS_COMMON) | $(BINDIR)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# Tests: build all test binaries
tests: ## Build all tests under test/
tests: $(TEST_BINS)

# Link rule for each test binary (link with common objects)
$(TESTBINDIR)/%: $(BUILDDIR)/test/%.o $(OBJS_COMMON) | $(TESTBINDIR)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# Compile steps with dep generation (mirror src/ -> build/)
$(BUILDDIR)/%.o: src/%.cc
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Compile tests with dep generation
$(BUILDDIR)/test/%.o: test/%.cc
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Ensure directories exist
$(BINDIR):
	@mkdir -p $@

$(TESTBINDIR):
	@mkdir -p $@

# Include auto-generated dependencies
-include $(DEPS) $(TEST_DEPS)

# Convenience targets
run-server: ## Build and run the server
run-server: $(BINDIR)/server
	$(BINDIR)/server

run-client: ## Build and run the client
run-client: $(BINDIR)/client
	$(BINDIR)/client

clean: ## Remove object files and dependency files
	$(RM) -r $(BUILDDIR)

distclean: ## Clean everything including binaries
	$(RM) -r $(BUILDDIR) $(BINDIR)

help: ## Show this help
	@grep -E '^[a-zA-Z0-9_/.-]+:.*?## ' $(lastword $(MAKEFILE_LIST)) | sed -E 's/:.*## /: /' | sort
