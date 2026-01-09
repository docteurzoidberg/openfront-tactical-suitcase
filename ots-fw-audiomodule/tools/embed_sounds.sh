#!/bin/bash
#
# embed_sounds.sh - Convert WAV files to embedded C arrays
#
# This script:
# 1. Scans sounds/*.wav files matching SD card naming (0000-9999.wav)
# 2. Converts them to 8-bit 22kHz mono format for flash embedding
# 3. Generates C header and source files for each sound
# 4. Places generated files in include/embedded/ and src/embedded/
#
# Usage:
#   ./tools/embed_sounds.sh
#   ./tools/embed_sounds.sh 0000 0001 0002  # Process specific sounds only
#
# Requirements:
#   - ffmpeg (for audio conversion)
#   - xxd (for binary to C array conversion)

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SOUNDS_DIR="$PROJECT_ROOT/sounds"
EMBEDDED_INC_DIR="$PROJECT_ROOT/include/embedded"
EMBEDDED_SRC_DIR="$PROJECT_ROOT/src/embedded"

# Audio format settings for embedded sounds
EMBED_SAMPLE_RATE=22050
EMBED_CHANNELS=1        # Mono
EMBED_BITS=8            # 8-bit
EMBED_FORMAT="u8"       # Unsigned 8-bit PCM

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}   OTS Audio Module - Embed Sounds Script${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""

# Check dependencies
if ! command -v ffmpeg &> /dev/null; then
    echo -e "${RED}Error: ffmpeg not found. Please install: sudo apt install ffmpeg${NC}"
    exit 1
fi

if ! command -v xxd &> /dev/null; then
    echo -e "${RED}Error: xxd not found. Please install: sudo apt install xxd${NC}"
    exit 1
fi

# Create output directories
mkdir -p "$EMBEDDED_INC_DIR"
mkdir -p "$EMBEDDED_SRC_DIR"

# Function to convert sound index to variable name
# Example: 0000 -> game_sound_0000, 0042 -> game_sound_0042
sound_id_to_varname() {
    local sound_id="$1"
    echo "game_sound_${sound_id}_22050_8bit"
}

# Function to get friendly name for game sounds
get_friendly_name() {
    local sound_id="$1"
    case "$sound_id" in
        0000) echo "game_start" ;;
        0001) echo "game_victory" ;;
        0002) echo "game_defeat" ;;
        0003) echo "game_death" ;;
        0004) echo "game_alert_nuke" ;;
        0005) echo "game_alert_land" ;;
        0006) echo "game_alert_naval" ;;
        0007) echo "game_nuke_launch" ;;
        10000) echo "test_tone_1s_440hz" ;;
        10001) echo "test_tone_2s_880hz" ;;
        10002) echo "test_tone_5s_220hz" ;;
        10100) echo "quack_sound" ;;
        *) echo "sound_${sound_id}" ;;
    esac
}

# Function to process a single WAV file
process_sound() {
    local input_file="$1"
    local sound_id="$2"
    local varname=$(sound_id_to_varname "$sound_id")
    local friendly_name=$(get_friendly_name "$sound_id")
    
    local temp_wav="$EMBEDDED_SRC_DIR/${varname}.wav"
    local output_c="$EMBEDDED_SRC_DIR/${varname}.c"
    local output_h="$EMBEDDED_INC_DIR/${varname}.h"
    
    echo -e "${YELLOW}Processing sound ${sound_id} (${friendly_name})...${NC}"
    echo "  Input:  $input_file"
    
    # Step 1: Convert to 8-bit 22kHz mono
    echo -n "  Converting to 8-bit 22kHz mono... "
    if ffmpeg -i "$input_file" \
        -ar "$EMBED_SAMPLE_RATE" \
        -ac "$EMBED_CHANNELS" \
        -acodec pcm_u8 \
        -y "$temp_wav" \
        -loglevel error; then
        echo -e "${GREEN}✓${NC}"
    else
        echo -e "${RED}✗ Failed${NC}"
        return 1
    fi
    
    # Get file size
    local file_size=$(stat -f%z "$temp_wav" 2>/dev/null || stat -c%s "$temp_wav" 2>/dev/null)
    
    # Step 2: Generate C header file
    echo -n "  Generating header... "
    cat > "$output_h" << EOF
/**
 * @file ${varname}.h
 * @brief Embedded sound: ${friendly_name} (ID ${sound_id})
 * 
 * Auto-generated from ${input_file}
 * Format: 8-bit PCM, 22050 Hz, Mono
 * Size: ${file_size} bytes
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint8_t ${varname}_wav[];
extern const size_t ${varname}_wav_len;

#ifdef __cplusplus
}
#endif
EOF
    echo -e "${GREEN}✓${NC}"
    
    # Step 3: Generate C source file with embedded data
    echo -n "  Generating source... "
    
    # Generate header
    cat > "$output_c" << EOF
/**
 * @file ${varname}.c
 * @brief Embedded sound data: ${friendly_name} (ID ${sound_id})
 * 
 * Auto-generated from ${input_file}
 * Format: 8-bit PCM, 22050 Hz, Mono
 * Size: ${file_size} bytes
 */

#include "embedded/${varname}.h"

EOF
    
    # Convert WAV to C array using xxd and wrap in array declaration
    echo "const uint8_t ${varname}_wav[] = {" >> "$output_c"
    xxd -i < "$temp_wav" | sed 's/^  //' >> "$output_c"
    echo "};" >> "$output_c"
    echo "" >> "$output_c"
    echo "const size_t ${varname}_wav_len = sizeof(${varname}_wav);" >> "$output_c"
    
    echo -e "${GREEN}✓${NC}"
    
    # Clean up temporary WAV file
    rm -f "$temp_wav"
    
    echo "  Output: $output_h"
    echo "  Output: $output_c"
    echo "  Size:   ${file_size} bytes"
    echo ""
    
    return 0
}

# Main processing
processed_count=0
failed_count=0

# Temporarily disable exit-on-error for the processing loop
set +e

# Determine which sounds to process
if [ $# -eq 0 ]; then
    # No arguments: process all WAV files in sounds/ directory
    echo -e "${BLUE}Scanning for WAV files in: $SOUNDS_DIR${NC}"
    echo ""
    
    if [ ! -d "$SOUNDS_DIR" ]; then
        echo -e "${RED}Error: sounds/ directory not found${NC}"
        echo "Expected: $SOUNDS_DIR"
        exit 1
    fi
    
    shopt -s nullglob
    sound_files=("$SOUNDS_DIR"/[0-9]*.wav)
    
    if [ ${#sound_files[@]} -eq 0 ]; then
        echo -e "${YELLOW}No WAV files found matching pattern: XXXX.wav or XXXXX.wav (0-99999)${NC}"
        exit 0
    fi
    
    for input_file in "${sound_files[@]}"; do
        filename=$(basename "$input_file")
        sound_id="${filename%.wav}"
        
        if process_sound "$input_file" "$sound_id"; then
            ((processed_count++))
        else
            ((failed_count++))
        fi
    done
else
    # Arguments provided: process specific sound IDs
    echo -e "${BLUE}Processing specified sounds: $@${NC}"
    echo ""
    
    for sound_id in "$@"; do
        # Ensure 4-digit format
        sound_id=$(printf "%04d" "$sound_id")
        input_file="$SOUNDS_DIR/${sound_id}.wav"
        
        if [ ! -f "$input_file" ]; then
            echo -e "${RED}Error: $input_file not found${NC}"
            ((failed_count++))
            continue
        fi
        
        if process_sound "$input_file" "$sound_id"; then
            ((processed_count++))
        else
            ((failed_count++))
        fi
    done
fi

# Summary
echo -e "${BLUE}================================================${NC}"
echo -e "${GREEN}✓ Processed: $processed_count sounds${NC}"
if [ $failed_count -gt 0 ]; then
    echo -e "${RED}✗ Failed:    $failed_count sounds${NC}"
fi
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo "1. Add includes to src/audio_player.c:"
echo "   #include \"embedded/game_sound_XXXX_22050_8bit.h\""
echo ""
echo "2. Add entries to embedded_game_sounds[] array:"
echo "   { XXXX, game_sound_XXXX_22050_8bit_wav, game_sound_XXXX_22050_8bit_wav_len, \"name\" }"
echo ""
echo "3. Rebuild firmware:"
echo "   pio run -e esp32-a1s-espidf"
echo -e "${BLUE}================================================${NC}"

exit 0
