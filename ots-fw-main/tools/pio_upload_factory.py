Import("env")

# PlatformIO's default ESP-IDF flashing behavior for an OTA partition table is to
# write the app image into ota_0. If otadata is erased/empty (our build generates
# an empty ota_data_initial.bin), the bootloader will fall back to the factory
# partition and keep running an older firmware.
#
# To guarantee that `pio run -t upload` always results in the flashed firmware
# actually booting, we explicitly flash the app image into the factory slot
# (0x10000) and also flash an empty otadata image (0xd000) to force bootloader
# fallback to factory.

env.Replace(
    UPLOADCMD=(
        '"$PYTHONEXE" "$UPLOADER" '
        '--chip $BOARD_MCU '
        '--port "$UPLOAD_PORT" '
        '--baud $UPLOAD_SPEED '
        '--before default-reset '
        '--after hard-reset '
        'write-flash -z '
        '--flash-mode dio '
        '--flash-freq 80m '
        '--flash-size detect '
        '0x0 "$BUILD_DIR/bootloader.bin" '
        '0x8000 "$BUILD_DIR/partitions.bin" '
        '0xd000 "$BUILD_DIR/ota_data_initial.bin" '
        '0x10000 "$BUILD_DIR/firmware.bin"'
    )
)
