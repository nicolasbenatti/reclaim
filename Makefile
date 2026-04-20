CC       := gcc
CXX      := g++
AR       := gcc-ar
CFLAGS   := -Wall -std=c11 -O3 -D_GNU_SOURCE -flto -march=native
CXXFLAGS := -Wall -std=c++17 -O3 -D_GNU_SOURCE
LDFLAGS  := -lpthread -flto

ASAN_FLAGS := -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-lto
TSAN_FLAGS := -O1 -fsanitize=thread -fno-omit-frame-pointer -fno-lto
CPPFLAGS := -Iinclude

LIB_NAME  := libreclaim.a
BENCH_BINS := bench_simple bench_mixed bench_malloc_large bench_threadtest bench_larson

SRC_DIR   := src
BENCH_DIR := benchmark
BUILD_DIR := build

SRC_SRCS := $(wildcard $(SRC_DIR)/*.c)
SRC_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_SRCS))

MAIN_OBJ   := $(BUILD_DIR)/main.o

.PHONY: all lib bench clean asan tsan

all: lib

asan:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(filter-out -O2,$(CFLAGS)) $(ASAN_FLAGS)"

tsan:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(filter-out -O2,$(CFLAGS)) $(TSAN_FLAGS)"

$(BUILD_DIR)/$(TARGET): $(MAIN_OBJ) $(SRC_OBJS)
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

lib: $(BUILD_DIR)/$(LIB_NAME)

$(BUILD_DIR)/$(LIB_NAME): $(SRC_OBJS)
	@mkdir -p $(BUILD_DIR)
	@$(AR) rcs $@ $^

bench: lib $(addprefix $(BUILD_DIR)/,$(BENCH_BINS))

$(BUILD_DIR)/bench_simple: $(BENCH_DIR)/simple.c $(BUILD_DIR)/$(LIB_NAME)
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CPPFLAGS) $(CFLAGS) $< -L$(BUILD_DIR) -lreclaim -lpthread -o $@

$(BUILD_DIR)/bench_mixed: $(BENCH_DIR)/mixed.c $(BUILD_DIR)/$(LIB_NAME)
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CPPFLAGS) $(CFLAGS) $< -L$(BUILD_DIR) -lreclaim -lpthread -o $@

$(BUILD_DIR)/bench_malloc_large: $(BENCH_DIR)/large.c $(BUILD_DIR)/$(LIB_NAME)
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CPPFLAGS) $(CFLAGS) $< -L$(BUILD_DIR) -lreclaim -lpthread -o $@

$(BUILD_DIR)/bench_threadtest: $(BENCH_DIR)/threadtest.c $(BUILD_DIR)/$(LIB_NAME)
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CPPFLAGS) $(CFLAGS) $< -L$(BUILD_DIR) -lreclaim -lpthread -o $@

$(BUILD_DIR)/bench_larson: $(BENCH_DIR)/larson.c $(BUILD_DIR)/$(LIB_NAME)
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CPPFLAGS) $(CFLAGS) $< -L$(BUILD_DIR) -lreclaim -lpthread -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(MAIN_OBJ): main.c
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR)
