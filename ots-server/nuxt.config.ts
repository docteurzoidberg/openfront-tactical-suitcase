// https://nuxt.com/docs/api/configuration/nuxt-config
export default defineNuxtConfig({
  compatibilityDate: '2025-07-15',
  devtools: { enabled: true },
  nitro: {
    experimental: {
      websocket: true
    }
  },
  css: ['~/assets/css/main.css'],
  ssr: false,
  modules: ['@nuxt/eslint', '@nuxt/ui', '@nuxt/fonts'],
  fonts: {
    families: [
      { name: 'Inter', provider: 'google', weights: [400, 500, 600, 700] }
    ],
    defaults: {
      sans: 'Inter, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'
    }
  }
})