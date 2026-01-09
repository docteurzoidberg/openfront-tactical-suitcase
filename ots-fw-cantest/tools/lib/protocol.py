#!/usr/bin/env python3
"""CAN bus protocol definitions and decoders for OTS cantest firmware.

This module provides:
- CAN ID constants (0x410-0x42F range)
- Message type enums
- Decode functions for discovery and audio protocols
- Validation functions
- Helper utilities for parsing CAN frames

Reference: /prompts/CANBUS_MESSAGE_SPEC.md
"""

from enum import IntEnum
from typing import Optional, Dict, Any, List
from dataclasses import dataclass


# ==============================================================================
# CAN ID Definitions
# ==============================================================================

class CANId(IntEnum):
    """CAN message IDs used in OTS protocol."""
    
    # Discovery Protocol (0x410-0x411)
    MODULE_ANNOUNCE = 0x410  # Module → Main: Identification response
    MODULE_QUERY = 0x411     # Main → Module: Query for modules
    
    # Audio Module Protocol (0x420-0x42F)
    PLAY_SOUND = 0x420       # Main → Audio: Play sound request
    STOP_SOUND = 0x421       # Main → Audio: Stop specific sound
    STOP_ALL = 0x422         # Main → Audio: Stop all sounds
    PLAY_SOUND_ACK = 0x423   # Audio → Main: Play acknowledgment
    STOP_SOUND_ACK = 0x424   # Audio → Main: Stop acknowledgment
    SOUND_FINISHED = 0x425   # Audio → Main: Sound completed


# ==============================================================================
# Module Types
# ==============================================================================

class ModuleType(IntEnum):
    """Module type identifiers."""
    NONE = 0x00
    AUDIO = 0x01
    # Future types: 0x02-0x7F
    # Custom/experimental: 0x80-0xFF


# ==============================================================================
# Capability Flags
# ==============================================================================

class ModuleCapability:
    """Module capability flags (bitfield)."""
    STATUS = 1 << 0   # 0x01 - Sends periodic status
    OTA = 1 << 1      # 0x02 - Supports firmware updates
    BATTERY = 1 << 2  # 0x04 - Battery powered


# ==============================================================================
# Audio Protocol Status Codes
# ==============================================================================

class AudioStatusCode(IntEnum):
    """Audio module status codes."""
    SUCCESS = 0x00
    FILE_NOT_FOUND = 0x01
    MIXER_FULL = 0x02
    SD_ERROR = 0x03
    UNKNOWN_ERROR = 0xFF


class SoundFinishReason(IntEnum):
    """Reason codes for SOUND_FINISHED message."""
    COMPLETED = 0x00
    STOPPED_BY_USER = 0x01
    PLAYBACK_ERROR = 0x02


# ==============================================================================
# Data Structures
# ==============================================================================

@dataclass
class CANFrame:
    """Represents a raw CAN frame."""
    can_id: int
    dlc: int  # Data Length Code (0-8)
    data: List[int]  # Byte array
    timestamp: Optional[float] = None
    
    def __str__(self) -> str:
        """Human-readable representation."""
        data_hex = ' '.join(f'{b:02X}' for b in self.data)
        return f"ID=0x{self.can_id:03X} DLC={self.dlc} [{data_hex}]"
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON output."""
        return {
            'can_id': f'0x{self.can_id:03X}',
            'can_id_dec': self.can_id,
            'dlc': self.dlc,
            'data': [f'0x{b:02X}' for b in self.data],
            'data_dec': self.data,
            'timestamp': self.timestamp
        }


@dataclass
class ModuleInfo:
    """Parsed MODULE_ANNOUNCE message."""
    module_type: int
    version_major: int
    version_minor: int
    capabilities: int
    can_block_base: int
    node_id: int
    
    def get_module_type_name(self) -> str:
        """Get human-readable module type name."""
        try:
            return ModuleType(self.module_type).name
        except ValueError:
            if self.module_type >= 0x80:
                return f"CUSTOM_0x{self.module_type:02X}"
            return f"UNKNOWN_0x{self.module_type:02X}"
    
    def get_capabilities_list(self) -> List[str]:
        """Get list of capability names."""
        caps = []
        if self.capabilities & ModuleCapability.STATUS:
            caps.append("STATUS")
        if self.capabilities & ModuleCapability.OTA:
            caps.append("OTA")
        if self.capabilities & ModuleCapability.BATTERY:
            caps.append("BATTERY")
        return caps
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON output."""
        return {
            'module_type': self.module_type,
            'module_type_name': self.get_module_type_name(),
            'version': f"{self.version_major}.{self.version_minor}",
            'version_major': self.version_major,
            'version_minor': self.version_minor,
            'capabilities': self.capabilities,
            'capabilities_list': self.get_capabilities_list(),
            'can_block_base': f"0x{self.can_block_base:02X}",
            'can_range': f"0x{self.can_block_base:02X}0-0x{self.can_block_base:02X}F",
            'node_id': self.node_id
        }


@dataclass
class PlaySoundRequest:
    """Parsed PLAY_SOUND message."""
    sound_index: int
    flags: int
    volume: int
    priority: int
    
    def is_loop(self) -> bool:
        """Check if loop flag is set."""
        return bool(self.flags & 0x01)
    
    def is_interrupt(self) -> bool:
        """Check if interrupt flag is set."""
        return bool(self.flags & 0x02)
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON output."""
        return {
            'sound_index': self.sound_index,
            'flags': self.flags,
            'loop': self.is_loop(),
            'interrupt': self.is_interrupt(),
            'volume': self.volume,
            'priority': self.priority
        }


@dataclass
class PlaySoundAck:
    """Parsed PLAY_SOUND_ACK message."""
    sound_index: int
    status: int
    queue_id: int
    
    def is_success(self) -> bool:
        """Check if playback started successfully."""
        return self.status == AudioStatusCode.SUCCESS
    
    def get_status_name(self) -> str:
        """Get human-readable status name."""
        try:
            return AudioStatusCode(self.status).name
        except ValueError:
            return f"UNKNOWN_0x{self.status:02X}"
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON output."""
        return {
            'sound_index': self.sound_index,
            'status': self.status,
            'status_name': self.get_status_name(),
            'success': self.is_success(),
            'queue_id': self.queue_id if self.is_success() else None
        }


@dataclass
class StopSoundRequest:
    """Parsed STOP_SOUND message."""
    queue_id: int
    
    def to_dict(self) -> Dict[str, Any]:
        return {'queue_id': self.queue_id}


@dataclass
class StopSoundAck:
    """Parsed STOP_SOUND_ACK message."""
    queue_id: int
    status: int
    
    def is_success(self) -> bool:
        return self.status == 0x00
    
    def to_dict(self) -> Dict[str, Any]:
        return {
            'queue_id': self.queue_id,
            'status': self.status,
            'success': self.is_success()
        }


@dataclass
class SoundFinished:
    """Parsed SOUND_FINISHED message."""
    queue_id: int
    sound_index: int
    reason: int
    
    def get_reason_name(self) -> str:
        """Get human-readable reason name."""
        try:
            return SoundFinishReason(self.reason).name
        except ValueError:
            return f"UNKNOWN_0x{self.reason:02X}"
    
    def to_dict(self) -> Dict[str, Any]:
        return {
            'queue_id': self.queue_id,
            'sound_index': self.sound_index,
            'reason': self.reason,
            'reason_name': self.get_reason_name()
        }


# ==============================================================================
# Decoder Functions
# ==============================================================================

def decode_module_query(frame: CANFrame) -> Optional[Dict[str, Any]]:
    """Decode MODULE_QUERY message (0x411).
    
    Format: [FF 00 00 00 00 00 00 00]
    Byte 0: Magic byte 0xFF (enumerate all modules)
    
    Returns:
        Dict with query info, or None if invalid
    """
    if frame.can_id != CANId.MODULE_QUERY or frame.dlc != 8:
        return None
    
    if frame.data[0] != 0xFF:
        return None
    
    return {
        'message_type': 'MODULE_QUERY',
        'magic': frame.data[0],
        'valid': True
    }


def decode_module_announce(frame: CANFrame) -> Optional[ModuleInfo]:
    """Decode MODULE_ANNOUNCE message (0x410).
    
    Format: [Type Ver_Maj Ver_Min Caps CAN_Block Node_ID 00 00]
    
    Returns:
        ModuleInfo object, or None if invalid
    """
    if frame.can_id != CANId.MODULE_ANNOUNCE or frame.dlc != 8:
        return None
    
    return ModuleInfo(
        module_type=frame.data[0],
        version_major=frame.data[1],
        version_minor=frame.data[2],
        capabilities=frame.data[3],
        can_block_base=frame.data[4],
        node_id=frame.data[5]
    )


def decode_play_sound(frame: CANFrame) -> Optional[PlaySoundRequest]:
    """Decode PLAY_SOUND message (0x420).
    
    Format: [Index Flags Volume Priority 00 00 00 00]
    
    Returns:
        PlaySoundRequest object, or None if invalid
    """
    if frame.can_id != CANId.PLAY_SOUND or frame.dlc != 8:
        return None
    
    return PlaySoundRequest(
        sound_index=frame.data[0],
        flags=frame.data[1],
        volume=frame.data[2],
        priority=frame.data[3]
    )


def decode_play_sound_ack(frame: CANFrame) -> Optional[PlaySoundAck]:
    """Decode PLAY_SOUND_ACK message (0x423).
    
    Format: [Index Status Queue_ID Reserved 00 00 00 00]
    
    Returns:
        PlaySoundAck object, or None if invalid
    """
    if frame.can_id != CANId.PLAY_SOUND_ACK or frame.dlc != 8:
        return None
    
    return PlaySoundAck(
        sound_index=frame.data[0],
        status=frame.data[1],
        queue_id=frame.data[2]
    )


def decode_stop_sound(frame: CANFrame) -> Optional[StopSoundRequest]:
    """Decode STOP_SOUND message (0x421).
    
    Format: [Queue_ID 00 00 00 00 00 00 00]
    
    Returns:
        StopSoundRequest object, or None if invalid
    """
    if frame.can_id != CANId.STOP_SOUND or frame.dlc != 8:
        return None
    
    return StopSoundRequest(queue_id=frame.data[0])


def decode_stop_sound_ack(frame: CANFrame) -> Optional[StopSoundAck]:
    """Decode STOP_SOUND_ACK message (0x424).
    
    Format: [Queue_ID Status 00 00 00 00 00 00]
    
    Returns:
        StopSoundAck object, or None if invalid
    """
    if frame.can_id != CANId.STOP_SOUND_ACK or frame.dlc != 8:
        return None
    
    return StopSoundAck(
        queue_id=frame.data[0],
        status=frame.data[1]
    )


def decode_stop_all(frame: CANFrame) -> Optional[Dict[str, Any]]:
    """Decode STOP_ALL message (0x422).
    
    Format: [00 00 00 00 00 00 00 00]
    
    Returns:
        Dict with message info, or None if invalid
    """
    if frame.can_id != CANId.STOP_ALL or frame.dlc != 8:
        return None
    
    return {
        'message_type': 'STOP_ALL',
        'valid': True
    }


def decode_sound_finished(frame: CANFrame) -> Optional[SoundFinished]:
    """Decode SOUND_FINISHED message (0x425).
    
    Format: [Queue_ID Index Reason 00 00 00 00 00]
    
    Returns:
        SoundFinished object, or None if invalid
    """
    if frame.can_id != CANId.SOUND_FINISHED or frame.dlc != 8:
        return None
    
    return SoundFinished(
        queue_id=frame.data[0],
        sound_index=frame.data[1],
        reason=frame.data[2]
    )


def decode_frame(frame: CANFrame) -> Dict[str, Any]:
    """Decode any CAN frame to structured data.
    
    Args:
        frame: CANFrame to decode
    
    Returns:
        Dictionary with decoded information including:
        - raw: Raw frame data
        - decoded: Decoded message (if known protocol)
        - message_type: Message type name (if known)
    """
    result = {
        'raw': frame.to_dict(),
        'decoded': None,
        'message_type': 'UNKNOWN'
    }
    
    # Try to decode based on CAN ID
    if frame.can_id == CANId.MODULE_QUERY:
        result['message_type'] = 'MODULE_QUERY'
        result['decoded'] = decode_module_query(frame)
    
    elif frame.can_id == CANId.MODULE_ANNOUNCE:
        result['message_type'] = 'MODULE_ANNOUNCE'
        module_info = decode_module_announce(frame)
        if module_info:
            result['decoded'] = module_info.to_dict()
    
    elif frame.can_id == CANId.PLAY_SOUND:
        result['message_type'] = 'PLAY_SOUND'
        play_req = decode_play_sound(frame)
        if play_req:
            result['decoded'] = play_req.to_dict()
    
    elif frame.can_id == CANId.PLAY_SOUND_ACK:
        result['message_type'] = 'PLAY_SOUND_ACK'
        play_ack = decode_play_sound_ack(frame)
        if play_ack:
            result['decoded'] = play_ack.to_dict()
    
    elif frame.can_id == CANId.STOP_SOUND:
        result['message_type'] = 'STOP_SOUND'
        stop_req = decode_stop_sound(frame)
        if stop_req:
            result['decoded'] = stop_req.to_dict()
    
    elif frame.can_id == CANId.STOP_SOUND_ACK:
        result['message_type'] = 'STOP_SOUND_ACK'
        stop_ack = decode_stop_sound_ack(frame)
        if stop_ack:
            result['decoded'] = stop_ack.to_dict()
    
    elif frame.can_id == CANId.STOP_ALL:
        result['message_type'] = 'STOP_ALL'
        result['decoded'] = decode_stop_all(frame)
    
    elif frame.can_id == CANId.SOUND_FINISHED:
        result['message_type'] = 'SOUND_FINISHED'
        finished = decode_sound_finished(frame)
        if finished:
            result['decoded'] = finished.to_dict()
    
    return result


# ==============================================================================
# Encoder Functions (for sending CAN messages)
# ==============================================================================

def encode_module_query() -> CANFrame:
    """Create MODULE_QUERY frame."""
    return CANFrame(
        can_id=CANId.MODULE_QUERY,
        dlc=8,
        data=[0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
    )


def encode_play_sound(sound_index: int, loop: bool = False, 
                     interrupt: bool = False, volume: int = 100, 
                     priority: int = 0) -> CANFrame:
    """Create PLAY_SOUND frame.
    
    Args:
        sound_index: Sound index (0-255)
        loop: Loop sound continuously
        interrupt: Stop all other sounds first
        volume: Volume (0-100)
        priority: Priority (0-255)
    
    Returns:
        CANFrame ready to send
    """
    flags = 0
    if loop:
        flags |= 0x01
    if interrupt:
        flags |= 0x02
    
    return CANFrame(
        can_id=CANId.PLAY_SOUND,
        dlc=8,
        data=[sound_index, flags, volume, priority, 0x00, 0x00, 0x00, 0x00]
    )


def encode_stop_sound(queue_id: int) -> CANFrame:
    """Create STOP_SOUND frame.
    
    Args:
        queue_id: Queue ID to stop (1-255)
    
    Returns:
        CANFrame ready to send
    """
    return CANFrame(
        can_id=CANId.STOP_SOUND,
        dlc=8,
        data=[queue_id, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
    )


def encode_stop_all() -> CANFrame:
    """Create STOP_ALL frame."""
    return CANFrame(
        can_id=CANId.STOP_ALL,
        dlc=8,
        data=[0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
    )


# ==============================================================================
# Validation Functions
# ==============================================================================

def is_valid_queue_id(queue_id: int) -> bool:
    """Check if queue ID is valid (1-255, 0 = invalid)."""
    return 1 <= queue_id <= 255


def is_valid_sound_index(sound_index: int) -> bool:
    """Check if sound index is valid (0-255)."""
    return 0 <= sound_index <= 255


def is_valid_volume(volume: int) -> bool:
    """Check if volume is valid (0-100)."""
    return 0 <= volume <= 100


def get_message_name(can_id: int) -> str:
    """Get human-readable message name from CAN ID.
    
    Args:
        can_id: CAN message ID
    
    Returns:
        Message name string
    """
    try:
        return CANId(can_id).name
    except ValueError:
        return f"UNKNOWN_0x{can_id:03X}"


# ==============================================================================
# Test/Demo Code
# ==============================================================================

if __name__ == '__main__':
    print("CAN Protocol Decoder Test")
    print("=" * 60)
    
    # Test MODULE_ANNOUNCE decoding
    print("\n1. MODULE_ANNOUNCE test:")
    announce_frame = CANFrame(
        can_id=0x410,
        dlc=8,
        data=[0x01, 0x01, 0x00, 0x01, 0x42, 0x00, 0x00, 0x00]
    )
    print(f"   Raw: {announce_frame}")
    decoded = decode_frame(announce_frame)
    print(f"   Type: {decoded['message_type']}")
    if decoded['decoded']:
        print(f"   Module: {decoded['decoded']['module_type_name']}")
        print(f"   Version: {decoded['decoded']['version']}")
        print(f"   CAN Range: {decoded['decoded']['can_range']}")
    
    # Test PLAY_SOUND encoding/decoding
    print("\n2. PLAY_SOUND test:")
    play_frame = encode_play_sound(sound_index=5, loop=True, volume=80)
    print(f"   Encoded: {play_frame}")
    decoded = decode_frame(play_frame)
    print(f"   Type: {decoded['message_type']}")
    if decoded['decoded']:
        print(f"   Sound: {decoded['decoded']['sound_index']}")
        print(f"   Loop: {decoded['decoded']['loop']}")
        print(f"   Volume: {decoded['decoded']['volume']}%")
    
    # Test PLAY_SOUND_ACK
    print("\n3. PLAY_SOUND_ACK test:")
    ack_frame = CANFrame(
        can_id=0x423,
        dlc=8,
        data=[0x05, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00]
    )
    print(f"   Raw: {ack_frame}")
    decoded = decode_frame(ack_frame)
    print(f"   Type: {decoded['message_type']}")
    if decoded['decoded']:
        print(f"   Status: {decoded['decoded']['status_name']}")
        print(f"   Queue ID: {decoded['decoded']['queue_id']}")
        print(f"   Success: {decoded['decoded']['success']}")
    
    print("\n" + "=" * 60)
    print("✓ Protocol decoder test complete")
