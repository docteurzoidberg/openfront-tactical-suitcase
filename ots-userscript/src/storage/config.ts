import { PROTOCOL_CONSTANTS } from '../../../ots-shared/src/game'
import { STORAGE_KEYS } from './keys'

const DEFAULT_WS_URL = PROTOCOL_CONSTANTS.DEFAULT_WS_URL

export function loadWsUrl(): string {
  const saved = GM_getValue<string | null>(STORAGE_KEYS.WS_URL, null)
  if (typeof saved === 'string' && saved.trim()) {
    return saved
  }
  return DEFAULT_WS_URL
}

export function saveWsUrl(url: string) {
  GM_setValue(STORAGE_KEYS.WS_URL, url)
}

export { DEFAULT_WS_URL }
