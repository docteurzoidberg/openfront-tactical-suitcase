<template>
  <div class="min-h-screen bg-slate-950 text-slate-100 p-6">
    <div class="mx-auto max-w-7xl space-y-6">
      <DashboardHeaderStatus
        :ui-status="uiStatus"
        :userscript-status="userscriptStatus"
        :userscript-blink="!!userscriptHeartbeatId"
        :game-status="gameStatus"
      />

      <section class="grid gap-6 lg:grid-cols-3">
        <!-- Column 1: Power + Troops -->
        <div class="space-y-6">
          <!-- Hardware Module: Main Power Module -->
          <HardwareMainPowerModule
            :connected="userscriptStatus === 'ONLINE'"
            :power-on="powerOn"
            @toggle-power="togglePower"
          />
          
          <!-- Hardware Module: Troops Module -->
          <HardwareTroopsModule
            :connected="uiStatus === 'OPEN'"
            :powered="powerOn"
            :troops="troops"
            :attack-ratio="attackRatio"
            :game-phase="gamePhase"
            @set-troops-percent="sendSetTroopsPercent"
          />
        </div>

        <!-- Column 2: Alert + Nuke -->
        <div class="space-y-6">
          <!-- Hardware Module: Alert Module -->
          <HardwareAlertModule
            :connected="uiStatus === 'OPEN'"
            :powered="powerOn"
            :active-alerts="activeAlerts"
          />

          <!-- Hardware Module: Nuke Control Panel -->
          <HardwareNukeModule
            :connected="uiStatus === 'OPEN'"
            :powered="powerOn"
            :game-phase="gamePhase"
            :active-nukes="activeNukes"
            @send-nuke="sendNukeCommand"
          />
        </div>

        <!-- Column 3: Events Log -->
        <div class="lg:sticky lg:top-6 lg:self-start">
          <DashboardEventsList :events="events" @clear-events="clearEvents" />
        </div>
      </section>
    </div>
  </div>
</template>

<script setup lang="ts">
import { useGameSocket } from '../composables/useGameSocket'

const { uiStatus, userscriptStatus, gameStatus, gamePhase, events, activeNukes, activeAlerts, powerOn, troops, attackRatio, sendNukeCommand, sendSetTroopsPercent, togglePower, userscriptHeartbeatId, clearEvents } = useGameSocket()
</script>
