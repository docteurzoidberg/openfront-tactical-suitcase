<template>
  <div class="keypad-grid">
    <!-- Row 1: K1-K7 -->
    <KeypadKeyButton
      v-for="keyId in [1, 2, 3, 4, 5, 6, 7]"
      :key="keyId"
      :keyId="keyId"
      :pressed="keyStates.get(keyId) || false"
      :led="ledStates.get(keyId)"
    />
    
    <!-- Row 2: K8-K14 -->
    <KeypadKeyButton
      v-for="keyId in [8, 9, 10, 11, 12, 13, 14]"
      :key="keyId"
      :keyId="keyId"
      :pressed="keyStates.get(keyId) || false"
      :led="ledStates.get(keyId)"
    />
    
    <!-- Row 3: K15 (Spacebar - wide key) -->
    <div class="col-span-1"></div> <!-- Spacer -->
    <KeypadKeyButton
      :keyId="15"
      :pressed="keyStates.get(15) || false"
      :led="ledStates.get(15)"
      :wide="true"
      class="col-span-5"
    />
    <div class="col-span-1"></div> <!-- Spacer -->
  </div>
</template>

<script setup lang="ts">
import type { LedState } from '~/types/keypad'
import KeypadKeyButton from './KeypadKeyButton.vue'

interface Props {
  keyStates: ReadonlyMap<number, boolean>
  ledStates: ReadonlyMap<number, LedState>
}

defineProps<Props>()
</script>

<style scoped>
.keypad-grid {
  display: grid;
  grid-template-columns: repeat(7, 1fr);
  gap: 0.5rem;
  padding: 1rem;
}
</style>
