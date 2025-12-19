<template>
  <div class="lcd-container">
    <div class="lcd-screen">
      <div class="lcd-content">
        <div class="lcd-line">{{ line1 }}</div>
        <div class="lcd-line">{{ line2 }}</div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import type { TroopsData, GamePhase } from '../../../../ots-shared/src/game'

interface Props {
  powered: boolean
  connected: boolean  // WebSocket connection
  gamePhase: GamePhase | null
  troops?: TroopsData | null
  sliderPercent?: number
}

const props = withDefaults(defineProps<Props>(), {
  troops: null,
  sliderPercent: 0
})

/**
 * Format troop count with K/M/B suffix and 1 decimal place
 * Based on firmware implementation in troops_module.c
 */
function formatTroopCount(troops: number): string {
  if (troops >= 1_000_000_000) {
    const billions = Math.floor(troops / 1_000_000_000)
    const decimal = Math.floor((troops % 1_000_000_000) / 100_000_000)
    return `${billions}.${decimal}B`
  } else if (troops >= 1_000_000) {
    const millions = Math.floor(troops / 1_000_000)
    const decimal = Math.floor((troops % 1_000_000) / 100_000)
    return `${millions}.${decimal}M`
  } else if (troops >= 1_000) {
    const thousands = Math.floor(troops / 1_000)
    const decimal = Math.floor((troops % 1_000) / 100)
    return `${thousands}.${decimal}K`
  }
  return troops.toString()
}

/**
 * Format Line 1 with right-aligned troop counts
 */
function formatTroopLine(current: number, max: number): string {
  const currentStr = formatTroopCount(current)
  const maxStr = formatTroopCount(max)
  const content = `${currentStr} / ${maxStr}`
  const padding = 16 - content.length
  return ' '.repeat(Math.max(0, padding)) + content
}

/**
 * Format Line 2 with left-aligned percentage and calculated troops
 */
function formatPercentLine(percent: number, currentTroops: number): string {
  const calculated = Math.floor(currentTroops * percent / 100)
  const calcStr = formatTroopCount(calculated)
  const content = `${percent}% (${calcStr})`
  return content.padEnd(16, ' ')
}

/**
 * Display logic based on firmware implementation
 * Follows DISPLAY_SCREENS_SPEC.md
 */
const line1 = computed(() => {
  if (!props.powered) {
    return '                ' // Blank screen when powered off
  }

  // Splash screen (shown briefly on initial load - handled by parent)
  // Would need a boot state to show this

  if (!props.connected) {
    // Waiting for connection screen
    return ' Waiting for    '
  }

  // Game phase determines screen
  switch (props.gamePhase) {
    case null:
      // Unknown state - waiting for connection
      return ' Waiting for    '
    
    case 'lobby':
      // Lobby screen
      return ' Connected!     '
    
    case 'spawning':
      // Spawning phase - countdown active
      return '   Spawning...  '
    
    case 'in-game':
      // Troops display (right-aligned)
      if (props.troops) {
        return formatTroopLine(props.troops.current, props.troops.max)
      }
      return '     --- / ---  '
    
    case 'game-won':
      // Victory screen
      return '   VICTORY!     '
    
    case 'game-lost':
      // Defeat screen
      return '    DEFEAT      '
    
    default:
      return ' Connected!     '
  }
})

const line2 = computed(() => {
  if (!props.powered) {
    return '                ' // Blank screen when powered off
  }

  if (!props.connected) {
    // Waiting for connection screen
    return ' Connection...  '
  }

  // Game phase determines screen
  switch (props.gamePhase) {
    case null:
      // Unknown state
      return ' Connection...  '
    
    case 'lobby':
      // Lobby screen
      return ' Waiting Game...'
    
    case 'spawning':
      // Spawning phase - countdown active
      return ' Get Ready!     '
    
    case 'in-game':
      // Troops display (left-aligned)
      if (props.troops) {
        return formatPercentLine(props.sliderPercent || 0, props.troops.current)
      }
      return '0% (---)        '
    
    case 'game-won':
    case 'game-lost':
      // Game end screen
      return ' Good Game!     '
    
    default:
      return ' Waiting Game...'
  }
})
</script>

<style scoped>
/* LCD 1602A Yellow-Green Display Styling */
/* Based on hardware spec: 16×2 character LCD with yellow-green backlight */
.lcd-container {
  display: inline-block;
  padding: 12px;
  background: linear-gradient(135deg, #2c3e50 0%, #34495e 100%);
  border-radius: 8px;
  box-shadow: 
    inset 0 2px 4px rgba(0, 0, 0, 0.4),
    0 4px 8px rgba(0, 0, 0, 0.3);
}

.lcd-screen {
  background: linear-gradient(180deg, #9db86d 0%, #a8c470 50%, #9db86d 100%);
  border: 2px solid #4a5c3a;
  border-radius: 4px;
  padding: 8px 12px;
  box-shadow: 
    inset 0 1px 3px rgba(0, 0, 0, 0.3),
    inset 0 -1px 2px rgba(255, 255, 255, 0.1);
  position: relative;
}

.lcd-screen::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: linear-gradient(
    180deg,
    rgba(255, 255, 255, 0.1) 0%,
    transparent 50%,
    rgba(0, 0, 0, 0.05) 100%
  );
  pointer-events: none;
  border-radius: 2px;
}

.lcd-content {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.lcd-line {
  font-family: 'LCD Matrix', 'Courier New', monospace;
  font-size: 18px;
  font-weight: 400;
  line-height: 1.3;
  letter-spacing: 0.5px;
  color: #2d3d1f;
  text-shadow: 0 0 2px rgba(45, 61, 31, 0.5);
  white-space: pre;
  font-feature-settings: 'tnum';
}

/* Each character should be monospaced */
.lcd-line::before {
  content: '';
  display: inline-block;
  width: 0;
}
</style>
