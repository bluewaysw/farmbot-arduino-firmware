TARGET_ramps_v16_BUILD_DIR := $(BUILD_DIR)/ramps_v16
TARGET_ramps_v16_HEX := $(BIN_DIR)/arduino16_firmware.hex

TARGET_ramps_v16_OBJ := $(patsubst $(FBARDUINO_FIRMWARE_SRC_DIR)/%,$(TARGET_ramps_v16_BUILD_DIR)/%,$(CXX_OBJ))

$(TARGET_ramps_v16_HEX): $(TARGET_ramps_v16_BUILD_DIR) $(TARGET_ramps_v16_BUILD_DIR)/arduino16_firmware.eep $(TARGET_ramps_v16_BUILD_DIR)/arduino16_firmware.elf
	$(OBJ_COPY) -O ihex -R .eeprom  $(TARGET_ramps_v16_BUILD_DIR)/arduino16_firmware.elf $@

$(TARGET_ramps_v16_BUILD_DIR)/arduino16_firmware.eep: $(TARGET_ramps_v16_BUILD_DIR)/arduino16_firmware.elf
	$(OBJ_COPY) -O ihex -j .eeprom --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0  $< $@

$(TARGET_ramps_v16_BUILD_DIR)/arduino_firmware.elf: $(TARGET_ramps_v16_OBJ)
	$(CC) -w -Os -g -flto -fuse-linker-plugin -Wl,--gc-sections,--relax -mmcu=atmega2560 -o $@ $(TARGET_ramps_v16_OBJ) $(DEPS_OBJ) $(DEP_CORE_LDFLAGS)

$(TARGET_ramps_v16_BUILD_DIR)/%.o: $(FBARDUINO_FIRMWARE_SRC_DIR)/%.cpp $(HEADERS)
	$(CXX) $(CXX_FLAGS) -DFARMBOT_BOARD_ID=0 $(DEPS_CFLAGS) $< -o $@

$(TARGET_ramps_v16_BUILD_DIR):
	$(MKDIR_P) $(TARGET_ramps_v16_BUILD_DIR)

target_ramps_v16: $(TARGET_ramps_v16_HEX)

target_ramps_v16_clean:
	$(RM) -r $(TARGET_ramps_v16_OBJ)
	$(RM) $(TARGET_ramps_v16_HEX)
