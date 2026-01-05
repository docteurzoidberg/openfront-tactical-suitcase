import { defineConfig } from 'vitepress'

// https://vitepress.dev/reference/site-config
export default defineConfig({
  title: "OpenFront Tactical Suitcase",
  description: "Hardware controller for OpenFront.io game",

  // GitHub Pages base path (repository name)
  base: '/openfront-tactical-suitcase/',

  // Ignore dead links from synced docs (many reference pages not yet created)
  ignoreDeadLinks: true,

  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: 'Home', link: '/' },
      { text: 'User Guide', link: '/user/' },
      { text: 'Developer', link: '/developer/' },
      { text: 'Downloads', link: '/downloads' },
      { text: 'Releases', link: '/releases' }
    ],

    sidebar: {
      '/user/': [
        {
          text: 'User Guide',
          items: [
            { text: 'Overview', link: '/user/' },
            { text: 'Quick Start', link: '/user/quick-start' },
            { text: 'WiFi Setup', link: '/user/wifi-setup' },
            { text: 'Userscript Install', link: '/user/userscript-install' }
          ]
        }
      ],
      '/developer/': [
        {
          text: 'Developer Guide',
          items: [
            { text: 'Overview', link: '/developer/' },
            { text: 'Getting Started', link: '/developer/getting-started' },
            { text: 'Repository Overview', link: '/developer/repository-overview' }
          ]
        }
      ]
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/docteurzoidberg/openfront-tactical-suitcase' }
    ],

    search: {
      provider: 'local'
    }
  }
})
