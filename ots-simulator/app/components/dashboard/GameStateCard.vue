<template>
  <UCard>
    <template #header>
      <div class="flex items-center justify-between">
        <h2 class="text-sm font-semibold text-slate-300">Current Game</h2>
      </div>
    </template>

    <div class="space-y-2 text-sm">
      <div class="flex flex-wrap items-center gap-3">
        <span class="rounded bg-slate-800 px-2 py-1 text-xs font-mono">
          {{ state ? new Date(state.timestamp).toLocaleTimeString() : '--:--:--' }}
        </span>
        <span class="font-medium">{{ state?.mapName || 'No game yet' }}</span>
        <span class="text-slate-400">· {{ state?.mode || 'Waiting for userscript' }}</span>
      </div>

      <div class="mt-2 flex flex-wrap items-center gap-3 text-sm">
        <span class="rounded bg-emerald-500/10 px-2 py-1 text-emerald-300">
          Team A: <span class="font-semibold">{{ state?.score.teamA ?? '-' }}</span>
        </span>
        <span class="rounded bg-sky-500/10 px-2 py-1 text-sky-300">
          Team B: <span class="font-semibold">{{ state?.score.teamB ?? '-' }}</span>
        </span>
      </div>

      <div class="mt-4">
        <h3 class="mb-2 text-xs font-semibold uppercase tracking-wide text-slate-400">Players</h3>
        <div class="max-h-64 overflow-auto rounded border border-slate-800/60 bg-slate-950/40">
          <table class="min-w-full text-xs">
            <thead class="bg-slate-900/80 text-slate-400">
              <tr>
                <th class="px-2 py-1 text-left font-medium">Name</th>
                <th class="px-2 py-1 text-left font-medium">Clan</th>
                <th class="px-2 py-1 text-center font-medium">Team</th>
                <th class="px-2 py-1 text-right font-medium">Score</th>
              </tr>
            </thead>
            <tbody v-if="state && state.players.length">
              <tr
                v-for="player in state.players"
                :key="player.id"
                class="border-t border-slate-900/80 hover:bg-slate-900/60"
              >
                <td class="px-2 py-1">
                  <span :class="player.isAlly ? 'text-emerald-300' : 'text-slate-200'">
                    {{ player.name }}
                  </span>
                </td>
                <td class="px-2 py-1 text-slate-400">
                  {{ player.clanTag ?? '-' }}
                </td>
                <td class="px-2 py-1 text-center">
                  <span
                    class="inline-flex rounded px-1.5 py-0.5 text-[10px] font-semibold uppercase tracking-wide"
                    :class="player.isAlly ? 'bg-emerald-500/15 text-emerald-300' : 'bg-rose-500/15 text-rose-300'"
                  >
                    {{ player.isAlly ? 'Ally' : 'Enemy' }}
                  </span>
                </td>
                <td class="px-2 py-1 text-right font-mono">
                  {{ player.score }}
                </td>
              </tr>
            </tbody>
            <tbody v-else>
              <tr>
                <td colspan="4" class="px-2 py-4 text-center text-xs text-slate-500">
                  Waiting for players from userscript…
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>
  </UCard>
</template>

<script setup lang="ts">
import type { GameState } from '../../../ots-shared/src/game'

const props = defineProps<{
  state: GameState | null
}>()

const { state } = toRefs(props)
</script>
