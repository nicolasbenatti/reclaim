CC       := gcc
AR       := ar
CFLAGS   := -Wall -std=c11 -O2
CFLAGS_WRAPALLOC   := -Wall -Wl,--wrap=malloc -Wl,--wrap=free -std=c11 -O2
CPPFLAGS := -Iinclude

TARGET   := reclaim
LIB_NAME := libreclaim.a

SRC_DIR  := src
BUILD_DIR  := build

SRC_SRCS := $(wildcard $(SRC_DIR)/*.c)
SRC_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_SRCS))

MAIN_OBJ := $(BUILD_DIR)/main.o

.PHONY: all lib clean

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(MAIN_OBJ) $(SRC_OBJS)
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CFLAGS) $^ -o $@

lib: $(BUILD_DIR)/$(LIB_NAME)

$(BUILD_DIR)/$(LIB_NAME): $(SRC_OBJS)
	@mkdir -p $(BUILD_DIR)
	@$(AR) rcs $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(MAIN_OBJ): main.c
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR)
