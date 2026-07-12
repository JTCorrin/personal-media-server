CC ?= gcc
BASE_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L \
	-Iinclude -Isrc -Ithird_party/mongoose
CFLAGS ?= $(BASE_CFLAGS)
LDFLAGS ?= -lsqlite3 -lpthread

ifeq ($(DEBUG),1)
CFLAGS += -g -O0
endif

UNITY_DIR := third_party/Unity/src
UNITY_SRC := $(UNITY_DIR)/unity.c
UNITY_CFLAGS := -I$(UNITY_DIR)

MONGOOSE_SRC := third_party/mongoose/mongoose.c
MONGOOSE_OBJ := build/third_party/mongoose.o
# Mongoose is a vendored amalgamation; keep its warnings quieter.
MONGOOSE_CFLAGS := -std=c11 -O2 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE \
	-Ithird_party/mongoose

TARGET := media-server
APP_SRCS := $(filter-out src/main.c,$(shell find src -name '*.c'))
APP_OBJS := $(APP_SRCS:src/%.c=build/%.o)
MAIN_OBJ := build/main.o

TEST_SRCS := $(wildcard tests/test_*.c)

# Suites included in `make test`. Add a name here when its UNIT_*_SRCS and tests are ready.
TEST_UNITS := log router routes config path media_kind catalog scanner
TEST_BINS := $(addprefix build/tests/test_,$(TEST_UNITS))

# Production sources linked into each test executable (UNIT_<name>_SRCS).
UNIT_log_SRCS := src/util/log.c src/util/log_sink_file.c src/util/log_sink_sqlite.c
UNIT_router_SRCS := src/http/router.c
UNIT_routes_SRCS := src/api/routes.c src/http/router.c src/http/server.c $(MONGOOSE_OBJ) \
	src/library/catalog.c src/media/kind.c src/util/path.c \
	src/util/log.c src/util/log_sink_file.c src/util/log_sink_sqlite.c
UNIT_config_SRCS := src/config.c src/util/log.c src/util/log_sink_file.c src/util/log_sink_sqlite.c
UNIT_path_SRCS := src/util/path.c
UNIT_media_kind_SRCS := src/media/kind.c src/util/path.c
UNIT_catalog_SRCS := src/library/catalog.c src/media/kind.c src/util/path.c
UNIT_scanner_SRCS := src/library/scanner.c src/library/catalog.c src/media/kind.c \
	src/util/path.c src/util/log.c src/util/log_sink_file.c src/util/log_sink_sqlite.c

.PHONY: all clean run test smoke smoke-library compile_commands.json

.SECONDEXPANSION:

all: compile_commands.json $(TARGET)

$(TARGET): $(MAIN_OBJ) $(APP_OBJS) $(MONGOOSE_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

$(MONGOOSE_OBJ): $(MONGOOSE_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(MONGOOSE_CFLAGS) -c $< -o $@

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/tests/test_%: tests/test_%.c $$(UNIT_%_SRCS) $(UNITY_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(UNITY_CFLAGS) $^ -o $@ $(LDFLAGS)

test: $(TEST_BINS)
	@failed=0; \
	for t in $(TEST_BINS); do \
		echo "==> $$t"; \
		$$t || failed=1; \
	done; \
	exit $$failed

smoke: $(TARGET)
	@chmod +x scripts/smoke_http.sh
	./scripts/smoke_http.sh

smoke-library: $(TARGET)
	@chmod +x scripts/smoke_library.sh
	./scripts/smoke_library.sh

compile_commands.json: $(APP_SRCS) src/main.c $(TEST_SRCS) Makefile
	@printf '[' > compile_commands.json
	@first=1; \
	for src in $(APP_SRCS) src/main.c; do \
		if [ $$first -eq 0 ]; then printf ',\n' >> compile_commands.json; fi; \
		first=0; \
		printf '  {"directory":"%s","command":"%s %s -c %s","file":"%s/%s"}' \
			"$(CURDIR)" "$(CC)" "$(CFLAGS)" "$$src" "$(CURDIR)" "$$src" >> compile_commands.json; \
	done; \
	for src in $(TEST_SRCS); do \
		if [ $$first -eq 0 ]; then printf ',\n' >> compile_commands.json; fi; \
		first=0; \
		printf '  {"directory":"%s","command":"%s %s %s -c %s","file":"%s/%s"}' \
			"$(CURDIR)" "$(CC)" "$(CFLAGS)" "$(UNITY_CFLAGS)" "$$src" "$(CURDIR)" "$$src" >> compile_commands.json; \
	done; \
	printf '\n]\n' >> compile_commands.json

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build $(TARGET)
