/**
 * Sound tab component - controls sound toggles and testing
 */
export class SoundTab {
  private container: HTMLElement

  constructor(
    private root: HTMLElement,
    private soundToggles: Record<string, boolean>,
    private logInfo: (text: string) => void,
    private onSoundTest?: (soundId: string) => void
  ) {
    this.container = root.querySelector('#ots-sound-toggles') as HTMLElement
    if (!this.container) {
      throw new Error('Sound tab container not found')
    }

    this.initializeToggles()
  }

  /**
   * Create HTML for sound tab
   */
  static createHTML(): string {
    return `
      <div id="ots-tab-sound" class="ots-tab-content" style="display:none;flex:1;overflow-y:auto;padding:8px;">
        <div id="ots-sound-toggles"></div>
      </div>
    `
  }

  private initializeToggles(): void {
    const soundIds = ['game_start', 'game_player_death', 'game_victory', 'game_defeat']
    const soundLabels: Record<string, string> = {
      game_start: 'Game Start',
      game_player_death: 'Player Death',
      game_victory: 'Victory',
      game_defeat: 'Defeat'
    }

    this.container.innerHTML = soundIds
      .map(
        (id) => `
      <div style="display:flex;align-items:center;gap:8px;margin-bottom:6px;padding:6px 8px;border-radius:4px;background:rgba(255,255,255,0.05);transition:background 0.2s;" data-sound-row="${id}">
        <label style="display:flex;align-items:center;gap:8px;font-size:11px;color:#e5e7eb;cursor:pointer;flex:1;">
          <input type="checkbox" data-sound-toggle="${id}" ${this.soundToggles[id] ? 'checked' : ''} style="cursor:pointer;" />
          <span style="flex:1;">${soundLabels[id]}</span>
          <code style="font-size:9px;color:#94a3b8;font-family:monospace;">${id}</code>
        </label>
        <button data-sound-test="${id}" style="all:unset;cursor:pointer;font-size:11px;padding:4px 10px;border-radius:4px;background:rgba(59,130,246,0.3);color:#93c5fd;font-weight:600;transition:background 0.2s;white-space:nowrap;">▶ Test</button>
      </div>
    `
      )
      .join('')

    this.attachListeners()
  }

  private attachListeners(): void {
    this.container.querySelectorAll('[data-sound-toggle]').forEach((checkbox) => {
      checkbox.addEventListener('change', (e) => {
        const input = e.target as HTMLInputElement
        const soundId = input.dataset.soundToggle!
        this.soundToggles[soundId] = input.checked
        this.logInfo(`Sound ${soundId}: ${this.soundToggles[soundId] ? 'enabled' : 'disabled'}`)
      })
    })

    this.container.querySelectorAll('[data-sound-test]').forEach((button) => {
      button.addEventListener('click', () => {
        const soundId = (button as HTMLElement).dataset.soundTest!
        this.logInfo(`Testing sound: ${soundId}`)
        this.onSoundTest?.(soundId)
      })

      button.addEventListener('mouseenter', () => {
        ; (button as HTMLElement).style.background = 'rgba(59,130,246,0.5)'
      })
      button.addEventListener('mouseleave', () => {
        ; (button as HTMLElement).style.background = 'rgba(59,130,246,0.3)'
      })
    })

    this.container.querySelectorAll('[data-sound-row]').forEach((row) => {
      row.addEventListener('mouseenter', () => {
        ; (row as HTMLElement).style.background = 'rgba(255,255,255,0.08)'
      })
      row.addEventListener('mouseleave', () => {
        ; (row as HTMLElement).style.background = 'rgba(255,255,255,0.05)'
      })
    })
  }
}
