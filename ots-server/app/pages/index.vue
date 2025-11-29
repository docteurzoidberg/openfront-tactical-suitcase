<template>
  <div class="min-h-screen bg-slate-950 text-slate-100 p-6">
    <div class="mx-auto max-w-5xl space-y-6">
      <DashboardHeaderStatus
        :status="status"
        :userscript-status="userscriptStatus"
        :userscript-blink="!!userscriptHeartbeatId"
      />

      <section class="grid gap-6 md:grid-cols-[2fr,1.5fr]">
        <div class="space-y-4">
          <DashboardGameStateCard :state="state" />

          <UCard>
            <template #header>
              <h2 class="text-sm font-semibold text-slate-300">Send Command</h2>
            </template>
            <form class="flex flex-wrap gap-2" @submit.prevent="sendCommand">
              <UInput
                v-model="command"
                type="text"
                size="sm"
                placeholder="e.g. focus-player:123 or ping"
                class="min-w-0 flex-1"
              />
              <UButton
                type="submit"
                size="sm"
                color="primary"
                :disabled="!command || status !== 'OPEN'"
                label="SEND"
              >
              </UButton>
            </form>
          </UCard>
        </div>

        <div class="space-y-4">
          <DashboardEventsList :events="events" />
        </div>
      </section>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from 'vue'
import { useGameSocket } from '../composables/useGameSocket'

const { status, userscriptStatus, latestState, events, send, userscriptHeartbeatId } = useGameSocket()

const state = computed(() => latestState.value)

const command = ref('')

const sendCommand = () => {
  if (!command.value.trim()) return
  send({ type: 'cmd', payload: { action: command.value.trim() } })
  command.value = ''
}
</script>
