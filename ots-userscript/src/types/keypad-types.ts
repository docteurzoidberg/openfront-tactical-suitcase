export type KeypadEventState = 'pressed' | 'released'

export type KeypadKeyEventData = {
  keyId: number
  state: KeypadEventState
  timestamp?: number
}

export type KeypadAction =
  | 'BUILD_CITY'
  | 'BUILD_FACTORY'
  | 'BUILD_PORT'
  | 'BUILD_DEFENSE'
  | 'BUILD_MISSILE'
  | 'BUILD_SAM'
  | 'BUILD_WARSHIP'
  | 'ZOOM_IN'
  | 'ZOOM_OUT'
  | 'ATTACK_DECREASE'
  | 'MISSILE_SWITCH'
  | 'ATTACK_INCREASE'
  | 'BOAT_ATTACK'
  | 'LAND_ATTACK'
  | 'TOGGLE_VIEW'

export type KeyBinding = {
  keyId: number
  action: KeypadAction
  selector: string
  label: string
  enabled: boolean
  hotkey: string
}

export type KeypadConfig = {
  version: number
  bindings: KeyBinding[]
}
