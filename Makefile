CC       := gcc
CXX      := g++
AR       := ar
CFLAGS   := -Wall -std=c11 -O2 -D_GNU_SOURCE
CXXFLAGS := -Wall -std=c++17 -O2 -D_GNU_SOURCE
LDFLAGS  := -lpthread

ASAN_FLAGS := -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
TSAN_FLAGS := -O1 -fsanitize=thread -fno-omit-frame-pointer
CPPFLAGS := -Iinclude

TARGET    := reclaim
LIB_NAME  := libreclaim.a
BENCH_BIN := libreclaim_benchmark

SRC_DIR   := src
BENCH_DIR := benchmark
BUILD_DIR := build

SRC_SRCS := $(wildcard $(SRC_DIR)/*.c)
SRC_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_SRCS))

MAIN_OBJ   := $(BUILD_DIR)/main.o

BENCH_SRCS := $(wildcard $(BENCH_DIR)/*.cc)
BENCH_OBJS := $(patsubst $(BENCH_DIR)/%.cc,$(BUILD_DIR)/bench_%.o,$(BENCH_SRCS))

.PHONY: all lib bench clean asan tsan

all: $(BUILD_DIR)/$(TARGET)

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

bench: lib $(BENCH_OBJS)
	@mkdir -p $(BUILD_DIR)
	@$(CXX) $(CXXFLAGS) $(BENCH_OBJS) -L$(BUILD_DIR) -lreclaim -lbenchmark -lpthread -o $(BUILD_DIR)/$(BENCH_BIN)

$(BUILD_DIR)/bench_%.o: $(BENCH_DIR)/%.cc
	@mkdir -p $(BUILD_DIR)
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(MAIN_OBJ): main.c
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR)
