/**
 * Game victory/defeat detection and event handling
 * Extracted from openfront-bridge.ts for better maintainability
 */

import type { WsClient } from '../websocket/client'
import type { Hud } from '../hud/sidebar-hud'
import { createLogger } from '../utils'

const logger = createLogger('VictoryHandler')

interface WinUpdate {
  winner?: [string, number | string]
  winnerType?: string
  winnerId?: number | string
  team?: number
  player?: string
  emoji?: {
    winner?: [string, number | string]
    recipientID?: number
  }
  [key: string]: unknown
}

interface VictoryContext {
  ws: WsClient
  hud: Hud
  myPlayer: {
    team?: () => number | null
    clientID?: () => string | null
    smallID?: () => number | null
  }
}

/**
 * Extract winner information from Win update object
 * Handles various Win update formats from OpenFront.io
 */
export function extractWinner(winUpdate: WinUpdate): { type: string | null, id: number | string | null } {
  let winnerType: string | null = null
  let winnerId: number | string | null = null

  // Strategy 1: winner array [type, id]
  if (Array.isArray(winUpdate.winner) && winUpdate.winner.length === 2) {
    winnerType = winUpdate.winner[0]
    winnerId = winUpdate.winner[1]
    logger.log('Found winner in update.winner:', winnerType, winnerId)
  }

  // Strategy 2: separate winnerType/winnerId fields
  else if (winUpdate.winnerType && (winUpdate.winnerId !== null && winUpdate.winnerId !== undefined)) {
    winnerType = winUpdate.winnerType
    winnerId = winUpdate.winnerId
    logger.log('Found winner in separate fields:', winnerType, winnerId)
  }

  // Strategy 3: emoji.winner array
  else if (winUpdate.emoji && Array.isArray(winUpdate.emoji.winner) && winUpdate.emoji.winner.length === 2) {
    winnerType = winUpdate.emoji.winner[0]
    winnerId = winUpdate.emoji.winner[1]
    logger.log('Found winner in emoji.winner:', winnerType, winnerId)
  }

  // Strategy 4: team field
  else if (winUpdate.team !== undefined && winUpdate.team !== null) {
    winnerType = 'team'
    winnerId = winUpdate.team
    logger.log('Found winner in update.team:', winnerId)
  }

  // Strategy 5: player field
  else if (winUpdate.player) {
    winnerType = 'player'
    winnerId = winUpdate.player
    logger.log('Found winner in update.player:', winnerId)
  }

  // Strategy 6: fallback to emoji.recipientID
  else if (winUpdate.emoji && winUpdate.emoji.recipientID !== undefined) {
    winnerType = 'player'
    winnerId = winUpdate.emoji.recipientID
    logger.log('Guessing winner from emoji.recipientID:', winnerId)
  }

  logger.log(`Final extracted winner - type: ${winnerType}, id: ${winnerId}`)
  return { type: winnerType, id: winnerId }
}

/**
 * Handle team game victory/defeat
 */
export function handleTeamVictory(ctx: VictoryContext, winnerId: number | string) {
  const myTeam = ctx.myPlayer.team ? ctx.myPlayer.team() : null
  const mySmallID = ctx.myPlayer.smallID ? ctx.myPlayer.smallID() : null

  logger.log(`My team: ${myTeam}, My smallID: ${mySmallID}, Winner ID: ${winnerId}`)

  if (myTeam !== null && winnerId === myTeam) {
    // Victory
    ctx.ws.sendEvent('GAME_END', 'Your team won!', {
      victory: true,
      phase: 'game-won',
      method: 'team-victory',
      myTeam,
      winnerId
    })

    if (ctx.hud.isSoundEnabled('game_victory')) {
      ctx.ws.sendEvent('SOUND_PLAY', 'Victory sound', {
        soundId: 'game_victory',
        priority: 'high'
      })
    }

    logger.success('Your team won!')
  } else if (myTeam !== null) {
    // Defeat
    ctx.ws.sendEvent('GAME_END', `Team ${winnerId} won`, {
      victory: false,
      phase: 'game-lost',
      method: 'team-defeat',
      myTeam,
      winnerId
    })

    if (ctx.hud.isSoundEnabled('game_defeat')) {
      ctx.ws.sendEvent('SOUND_PLAY', 'Defeat sound', {
        soundId: 'game_defeat',
        priority: 'high'
      })
    }

    logger.failure(`Team ${winnerId} won (you are team ${myTeam})`)
  } else {
    // Fallback: team is null
    logger.warn('myPlayer.team() returned null, assuming defeat')
    ctx.ws.sendEvent('GAME_END', `Team ${winnerId} won`, {
      victory: false,
      phase: 'game-lost',
      method: 'team-defeat-fallback',
      myTeam: null,
      winnerId
    })

    if (ctx.hud.isSoundEnabled('game_defeat')) {
      ctx.ws.sendEvent('SOUND_PLAY', 'Defeat sound', {
        soundId: 'game_defeat',
        priority: 'high'
      })
    }

    logger.failure(`Team ${winnerId} won (unable to determine your team)`)
  }
}

/**
 * Handle solo game victory/defeat
 */
export function handleSoloVictory(ctx: VictoryContext, winnerId: number | string) {
  const myClientID = ctx.myPlayer.clientID ? ctx.myPlayer.clientID() : null
  const mySmallID = ctx.myPlayer.smallID ? ctx.myPlayer.smallID() : null

  logger.log(`My clientID: ${myClientID}, My smallID: ${mySmallID}, Winner ID: ${winnerId}`)

  if (myClientID !== null && winnerId === myClientID) {
    // Victory
    ctx.ws.sendEvent('GAME_END', 'You won!', {
      victory: true,
      phase: 'game-won',
      method: 'solo-victory',
      myClientID,
      winnerId
    })

    if (ctx.hud.isSoundEnabled('game_victory')) {
      ctx.ws.sendEvent('SOUND_PLAY', 'Victory sound', {
        soundId: 'game_victory',
        priority: 'high'
      })
    }

    logger.success('You won!')
  } else {
    // Defeat
    ctx.ws.sendEvent('GAME_END', 'Another player won', {
      victory: false,
      phase: 'game-lost',
      method: 'solo-defeat',
      myClientID,
      winnerId
    })

    if (ctx.hud.isSoundEnabled('game_defeat')) {
      ctx.ws.sendEvent('SOUND_PLAY', 'Defeat sound', {
        soundId: 'game_defeat',
        priority: 'high'
      })
    }

    logger.failure('Another player won')
  }
}

/**
 * Process Win update and determine victory/defeat
 * Returns true if win was processed, false otherwise
 */
export function processWinUpdate(
  ctx: VictoryContext,
  winUpdate: WinUpdate
): boolean {
  const { type: winnerType, id: winnerId } = extractWinner(winUpdate)

  if (!winnerType || winnerId === null || winnerId === undefined) {
    logger.warn('Win update has no winner field:', winUpdate)
    return false
  }

  if (winnerType === 'team') {
    handleTeamVictory(ctx, winnerId)
  } else if (winnerType === 'player') {
    handleSoloVictory(ctx, winnerId)
  } else {
    logger.warn('Unknown winner type:', winnerType)
    return false
  }

  return true
}
