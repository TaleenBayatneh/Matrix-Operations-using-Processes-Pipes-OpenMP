# Matrix project — Always compiled with OpenMP; runtime toggle via menu
CC      := gcc
STD     := -std=c11
WARN    := -Wall -Wextra -Wpedantic
DBG     ?= 1        # default: debug build when running `make`
SAN     ?= 0

CFLAGS  := $(STD) $(WARN) -MMD -MP
LDFLAGS := -lm
INC     := -Iinclude

# Debug/Release flags
ifeq ($(DBG),1)
  CFLAGS += -O0 -g
else
  CFLAGS += -O2
endif

# Sanitizers (optional)
ifeq ($(SAN),1)
  CFLAGS  += -fsanitize=address,undefined
  LDFLAGS += -fsanitize=address,undefined
endif

# Always compile with OpenMP available; runtime toggle is in the menu
CFLAGS  += -fopenmp -DHAVE_OMP
LDFLAGS += -fopenmp

SRC := \
  src/main.c \
  src/menu.c \
  src/matrix.c \
  src/ops_addsub.c \
  src/ops_mul.c \
  src/ops_det_eig.c \
  src/file_io.c \
  src/pool_workers.c \
  src/timer.c

OBJ   := $(patsubst src/%.c, build/%.o, $(SRC))
DEPS  := $(OBJ:.o=.d)
BIN   := bin/matrix-project

.PHONY: all run clean dirs info help

all: dirs info $(BIN)


$(BIN): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

run: all
	@./$(BIN) --config config/default.conf

dirs:
	@mkdir -p bin build

clean:
	@rm -rf build bin
	
-include $(DEPS)

help:
	@echo "Targets:"
	@echo "  make        # debug build (default)"
	@echo "  make run    # build + run"
	@echo "  make clean  # clean artifacts"
