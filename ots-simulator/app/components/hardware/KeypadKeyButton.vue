<template>
  <button
    :class="[
      'relative flex items-center justify-center',
      'bg-slate-700 border-2 border-slate-600 rounded',
      'h-16 transition-all duration-100',
      'cursor-default select-none',
      pressed && 'scale-95 bg-slate-500',
      led?.on && 'border-4',
      wide && 'col-span-4'
    ]"
    :style="ledStyle"
  >
    <div class="text-sm font-medium text-slate-200">K{{ keyId }}</div>
    <div 
      v-if="led?.on" 
      class="absolute top-1 right-1 w-2 h-2 rounded-full"
      :style="ledColorStyle"
    ></div>
  </button>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import type { LedState } from '~/types/keypad'

interface Props {
  keyId: number
  pressed?: boolean
  led?: LedState
  wide?: boolean
}

const props = defineProps<Props>()

const ledStyle = computed(() => {
  if (!props.led?.on) return {}
  
  const { r, g, b } = props.led.color
  return {
    borderColor: `rgb(${r}, ${g}, ${b})`,
    boxShadow: `0 0 10px rgba(${r}, ${g}, ${b}, 0.5)`
  }
})

const ledColorStyle = computed(() => {
  if (!props.led?.on) return {}
  
  const { r, g, b } = props.led.color
  return {
    backgroundColor: `rgb(${r}, ${g}, ${b})`
  }
})
</script>
