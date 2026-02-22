GCC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Iheaders -I/usr/include/SDL2 -D_REENTRANT
LDFLAGS = -lSDL2 -lSDL2_ttf

BUILD_DIR = bin
SRC_DIR = src

SRC = $(wildcard $(SRC_DIR)/*.c)

BIN_MAIN = $(BUILD_DIR)/main
BIN_ASSET_DRAWER = $(BUILD_DIR)/asset_drawer

all: main asset-drawer

main:
	$(GCC) $(SRC) $(CFLAGS) -o $(BIN_MAIN) $(LDFLAGS)

asset-drawer:
	$(GCC) $(SRC_DIR)/asset_drawer.c $(CFLAGS) -o $(BIN_ASSET_DRAWER) $(LDFLAGS)

run: main
	./$(BIN_MAIN)

run-asset-drawer: asset-drawer
	./$(BIN_ASSET_DRAWER) 128 128 -l "assets/spritesheet.him" -o "assets/spritesheet.him"
