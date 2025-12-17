import { PROTOCOL_CONSTANTS } from '../../../ots-shared/src/game'

const DEFAULT_WS_URL = PROTOCOL_CONSTANTS.DEFAULT_WS_URL
const STORAGE_KEY_WS_URL = 'ots-ws-url'

export function loadWsUrl(): string {
  const saved = GM_getValue<string | null>(STORAGE_KEY_WS_URL, null)
  if (typeof saved === 'string' && saved.trim()) {
    return saved
  }
  return DEFAULT_WS_URL
}

export function saveWsUrl(url: string) {
  GM_setValue(STORAGE_KEY_WS_URL, url)
}

export { DEFAULT_WS_URL }
