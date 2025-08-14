# Makefile for ByoRedis
# Usage examples:
#   make                # build all (server, client)
#   make -j             # parallel build
#   make run-server     # run built server
#   make DEBUG=1        # debug build (-O0 -g3)
#   make SAN=address    # enable sanitizer(s), e.g., address,undefined

# Tools and flags (override from CLI if needed)
CXX ?= g++
CPPFLAGS ?=
CPPFLAGS += -I.

# Base C++ flags
CXXFLAGS ?=
CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic -O2 -g

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

# Sources
SRCS := common.cc client.cc server.cc server_conn.cc client_api.cc hashtable.cc
OBJS := $(SRCS:%.cc=$(BUILDDIR)/%.o)
DEPS := $(OBJS:.o=.d)

SERVER_OBJS := $(BUILDDIR)/server.o $(BUILDDIR)/common.o $(BUILDDIR)/server_conn.o $(BUILDDIR)/hashtable.o
CLIENT_OBJS := $(BUILDDIR)/client.o $(BUILDDIR)/common.o $(BUILDDIR)/client_api.o

# Default goal
.DEFAULT_GOAL := all

# Phony targets
.PHONY: all clean distclean run-server run-client help

all: ## Build all targets (server, client)
all: $(BINDIR)/server $(BINDIR)/client

# Link steps
$(BINDIR)/server: $(SERVER_OBJS) | $(BINDIR)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BINDIR)/client: $(CLIENT_OBJS) | $(BINDIR)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# Compile steps with dep generation
$(BUILDDIR)/%.o: %.cc | $(BUILDDIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Ensure directories exist
$(BUILDDIR):
	@mkdir -p $@

$(BINDIR):
	@mkdir -p $@

# Include auto-generated dependencies
-include $(DEPS)

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
	@grep -E '^[a-zA-Z0-9_-]+:.*?## ' $(lastword $(MAKEFILE_LIST)) | sed -E 's/:.*## /: /' | sort
