export function waitForElement(selector: string, callback: (el: Element) => void) {
  const existing = document.querySelector(selector)
  if (existing) {
    callback(existing)
    return
  }

  const observer = new MutationObserver(() => {
    const el = document.querySelector(selector)
    if (el) {
      observer.disconnect()
      callback(el)
    }
  })

  observer.observe(document.documentElement || document.body, {
    childList: true,
    subtree: true
  })
}

export function waitForGameInstance(controlPanel: any, callback: (game: any) => void, checkInterval = 100) {
  const existing = (controlPanel as any).game
  if (existing) {
    callback(existing)
    return
  }

  const intervalId = window.setInterval(() => {
    const game = (controlPanel as any).game
    if (game) {
      window.clearInterval(intervalId)
      callback(game)
    }
  }, checkInterval)
}
