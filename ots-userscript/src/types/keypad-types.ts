export type KeypadEventState = 'pressed' | 'released'

export type KeypadKeyEventData = {
  keyId: number
  state: KeypadEventState
  timestamp?: number
}

export type KeypadAction = string

export type KeyBinding = {
  keyId: number
  action: KeypadAction
  label: string
  enabled: boolean
}

export type KeypadConfig = {
  version: number
  bindings: KeyBinding[]
}
