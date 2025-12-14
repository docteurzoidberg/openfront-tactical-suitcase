<template>
  <div class="min-h-screen bg-slate-950 text-slate-100 p-6">
    <div class="mx-auto max-w-7xl space-y-6">
      <DashboardHeaderStatus
        :ui-status="uiStatus"
        :userscript-status="userscriptStatus"
        :userscript-blink="!!userscriptHeartbeatId"
      />

      <section class="grid gap-6 lg:grid-cols-2">
        <!-- Column 1: Hardware Modules -->
        <div class="space-y-6">
          <!-- Hardware Module: Main Power Module -->
          <HardwareMainPowerModule
            :connected="userscriptStatus === 'ONLINE'"
            :power-on="powerOn"
            @toggle-power="togglePower"
          />

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
            :active-nukes="activeNukes"
            @send-nuke="sendNukeCommand"
          />
        </div>

        <!-- Column 2: Events Log -->
        <div class="lg:sticky lg:top-6 lg:self-start">
          <DashboardEventsList :events="events" />
        </div>
      </section>
    </div>
  </div>
</template>

<script setup lang="ts">
import { useGameSocket } from '../composables/useGameSocket'

const { uiStatus, userscriptStatus, events, activeNukes, activeAlerts, powerOn, sendNukeCommand, togglePower, userscriptHeartbeatId } = useGameSocket()
</script>
