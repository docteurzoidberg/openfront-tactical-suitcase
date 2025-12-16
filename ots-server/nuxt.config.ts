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
      { name: 'LCD Matrix', src: '/fonts/Tims_8x5_LCD_Matrix.ttf' }
    ]
  }
})