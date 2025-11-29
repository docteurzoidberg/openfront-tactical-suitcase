import { getLatestState } from '../../routes/ws-script'

export default defineEventHandler(() => {
  return getLatestState()
})
