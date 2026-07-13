CC ?= gcc
# -MMD -MP emits a .d makefile fragment per object so header edits trigger
# rebuilds of the objects that include them (see the -include at the bottom).
BASE_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2 -MMD -MP \
	-D_POSIX_C_SOURCE=200809L \
	-Iinclude -Isrc -Ithird_party/mongoose
CFLAGS ?= $(BASE_CFLAGS)
# LDFLAGS = linker options (e.g. -fsanitize), LDLIBS = libraries. Keeping them
# separate matters because libraries must come after objects on the link line.
LDFLAGS ?=
LDLIBS ?= -lsqlite3 -lpthread

ifeq ($(DEBUG),1)
CFLAGS += -g -O0
endif

# Appended after any DEBUG handling; used by the test-asan target.
CFLAGS += $(EXTRA_CFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

SANITIZE_FLAGS := -g -O1 -fsanitize=address,undefined

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
TEST_UNITS := log router routes config path media_kind catalog scanner media_file \
	string_buf path_meta browse str params search runtime catalog_store
TEST_BINS := $(addprefix build/tests/test_,$(TEST_UNITS))

# Production sources linked into each test executable (UNIT_<name>_SRCS).
UNIT_log_SRCS := src/util/log.c src/util/log_sink_file.c src/util/log_sink_sqlite.c
UNIT_router_SRCS := src/http/router.c
UNIT_routes_SRCS := src/api/routes.c src/api/ping.c src/api/tracks.c src/api/images.c \
	src/api/artists.c src/api/albums.c src/api/search.c src/api/library.c \
	src/api/media_serve.c src/api/stub.c src/api/params.c src/api/catalog_json.c \
	src/api/page_json.c src/api/browse_json.c \
	src/http/router.c src/http/server.c $(MONGOOSE_OBJ) \
	src/library/catalog.c src/library/browse.c src/library/search.c \
	src/library/scanner.c src/library/runtime.c src/library/catalog_store.c \
	src/media/kind.c src/media/file.c src/media/path_meta.c \
	src/util/path.c src/util/string_buf.c src/util/str.c \
	src/util/log.c src/util/log_sink_file.c src/util/log_sink_sqlite.c
UNIT_config_SRCS := src/config.c src/util/log.c src/util/log_sink_file.c src/util/log_sink_sqlite.c
UNIT_path_SRCS := src/util/path.c
UNIT_path_meta_SRCS := src/media/path_meta.c src/util/path.c
UNIT_media_kind_SRCS := src/media/kind.c src/util/path.c
UNIT_media_file_SRCS := src/media/file.c src/media/kind.c src/util/path.c
UNIT_catalog_SRCS := src/library/catalog.c src/media/kind.c src/media/path_meta.c \
	src/util/path.c
UNIT_catalog_store_SRCS := src/library/catalog_store.c src/library/catalog.c \
	src/media/kind.c src/media/path_meta.c src/util/path.c \
	src/util/log.c src/util/log_sink_file.c src/util/log_sink_sqlite.c
UNIT_browse_SRCS := src/library/browse.c src/library/catalog.c src/media/kind.c \
	src/media/path_meta.c src/util/path.c
UNIT_search_SRCS := src/library/search.c src/library/catalog.c src/library/browse.c \
	src/media/kind.c src/media/path_meta.c src/util/path.c src/util/str.c
UNIT_scanner_SRCS := src/library/scanner.c src/library/catalog.c src/media/kind.c \
	src/media/path_meta.c src/util/path.c src/util/log.c src/util/log_sink_file.c \
	src/util/log_sink_sqlite.c
UNIT_runtime_SRCS := src/library/runtime.c src/library/scanner.c src/library/catalog.c \
	src/library/browse.c src/library/catalog_store.c src/media/kind.c \
	src/media/path_meta.c src/util/path.c \
	src/util/log.c src/util/log_sink_file.c src/util/log_sink_sqlite.c
UNIT_string_buf_SRCS := src/util/string_buf.c
UNIT_str_SRCS := src/util/str.c
UNIT_params_SRCS := src/api/params.c src/http/server.c $(MONGOOSE_OBJ) \
	src/http/router.c src/util/log.c src/util/log_sink_file.c src/util/log_sink_sqlite.c

.PHONY: all clean run test test-asan smoke smoke-library smoke-stream smoke-api compile_commands.json

.SECONDEXPANSION:

all: compile_commands.json $(TARGET)

$(TARGET): $(MAIN_OBJ) $(APP_OBJS) $(MONGOOSE_OBJ)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(MONGOOSE_OBJ): $(MONGOOSE_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(MONGOOSE_CFLAGS) -c $< -o $@

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/tests/test_%: tests/test_%.c $$(UNIT_%_SRCS) $(UNITY_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(UNITY_CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

test: $(TEST_BINS)
	@failed=0; \
	for t in $(TEST_BINS); do \
		echo "==> $$t"; \
		$$t || failed=1; \
	done; \
	exit $$failed

# Rebuild everything with AddressSanitizer + UBSan and run the tests.
# Cleans first (and afterwards leaves sanitized objects behind), so run
# `make clean` before going back to normal builds.
test-asan:
	$(MAKE) clean
	$(MAKE) test EXTRA_CFLAGS="$(SANITIZE_FLAGS)" EXTRA_LDFLAGS="-fsanitize=address,undefined"

smoke: $(TARGET)
	@chmod +x scripts/smoke_http.sh
	./scripts/smoke_http.sh

smoke-library: $(TARGET)
	@chmod +x scripts/smoke_library.sh
	./scripts/smoke_library.sh

smoke-stream: $(TARGET)
	@chmod +x scripts/smoke_stream.sh
	./scripts/smoke_stream.sh

smoke-api: $(TARGET)
	@chmod +x scripts/smoke_api.sh
	./scripts/smoke_api.sh

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

# Auto-generated header dependencies (from -MMD -MP). The leading '-' means
# "no error if they don't exist yet" (e.g. first build after clean).
-include $(APP_OBJS:.o=.d) $(MAIN_OBJ:.o=.d)
