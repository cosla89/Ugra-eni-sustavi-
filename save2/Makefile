MCU = atmega328p
F_CPU = 16000000UL
BAUD = 115200
PORT = COM4

CC = avr-gcc
OBJCOPY = avr-objcopy
AVRDUDE = "C:/Users/Korisnik/AppData/Local/Arduino15/packages/arduino/tools/avrdude/8.0.0-arduino1/bin/avrdude.exe"
CONF = "C:/Users/Korisnik/AppData/Local/Arduino15/packages/arduino/tools/avrdude/8.0.0-arduino1/etc/avrdude.conf"

# folders
SRC_DIR = src
BUILD_DIR = build

# find all C files
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

CFLAGS = -std=gnu99 -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Iinclude -Idrivers

TARGET = $(BUILD_DIR)/main

all: hex

# create build folder
$(BUILD_DIR):
	mkdir $(BUILD_DIR)

# compile each .c file
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# link
$(TARGET).elf: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

# hex
hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $(TARGET).elf $(TARGET).hex

# flash
flash: hex
	$(AVRDUDE) -C $(CONF) -c arduino -p $(MCU) -P $(PORT) -b $(BAUD) -U flash:w:$(TARGET).hex:i

# clean
clean:
	del /Q build\*.o build\*.elf build\*.hex
