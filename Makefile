CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L -Iinclude -Isrc
LDFLAGS ?= -lsqlite3 -lpthread

UNITY_DIR := third_party/Unity/src
UNITY_SRC := $(UNITY_DIR)/unity.c
UNITY_CFLAGS := -I$(UNITY_DIR)

TARGET := media-server
SRCS := $(shell find src -name '*.c')
OBJS := $(SRCS:src/%.c=build/%.o)

TEST_SRCS := $(wildcard tests/test_*.c)
TEST_BINS := $(TEST_SRCS:tests/test_%.c=build/tests/test_%)

# Production sources linked into each test executable (UNIT_<name>_SRCS).
UNIT_log_SRCS := src/util/log.c src/util/log_sink_file.c src/util/log_sink_sqlite.c

.PHONY: all clean run test compile_commands.json

.SECONDEXPANSION:

all: compile_commands.json $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

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

compile_commands.json: $(SRCS) $(TEST_SRCS) Makefile
	@printf '[' > compile_commands.json
	@first=1; \
	for src in $(SRCS) $(TEST_SRCS); do \
		if [ $$first -eq 0 ]; then printf ',\n' >> compile_commands.json; fi; \
		first=0; \
		printf '  {"directory":"%s","command":"%s %s -c %s","file":"%s/%s"}' \
			"$(CURDIR)" "$(CC)" "$(CFLAGS)" "$$src" "$(CURDIR)" "$$src" >> compile_commands.json; \
	done; \
	printf '\n]\n' >> compile_commands.json

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build $(TARGET)
