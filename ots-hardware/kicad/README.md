Here are the kicad projects used to make the Openfront Tactical Suitcae PCBS.

# Openfront Tactical Suitcase - Main PCB 

//TODO: Describe the main PCB here. What it does, what components are on it, etc.

//TODO: INSERT SCREENSHOT OF MAIN PCB HERE

[controller_rev2.zip](controller_rev2.zip) 

# Openfront Tactical Suitcase - IO PCB

//TODO: Describe the IO PCB here. What it does, what components are on it, etc.

//TODO: INSERT SCREENSHOT OF IO PCB HERE 

[moduleboard_rev1.zip](moduleboard_rev1.zip)

# Openfront Tactical Suitcase - Keypad PCB 

The Keypad Module is a dual-mode 15-key mechanical keyboard with RGB backlighting. It operates as either a CAN bus module integrated into the OTS suitcase or as a standalone USB HID keyboard.

**Features:**
- 15x Cherry MX-compatible switches (plate-mount, soldered)
- 15x SK6812-MINI-E RGB LEDs (reverse mounted)
- M5Stack Stamp S3 (ESP32-S3) controller
- TJA1050 CAN transceiver
- USB-C connector
- Dual-mode: CAN bus (suitcase) or USB HID (standalone)

**Layout:** 2 rows of 7 keys + 1 centered 2U spacebar

**Assembly:** Switches mount to front panel plate first, then solder to PCB

**Specification:** See `../modules/keypad-module.md` for full details

//TODO: INSERT SCREENSHOT OF KEYPAD PCB HERE

[keyboard_rev1.zip](keyboard_rev1.zip)
