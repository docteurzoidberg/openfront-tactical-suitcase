import esbuild from 'esbuild'

await esbuild.build({
  entryPoints: ['src/main.user.ts'],
  bundle: true,
  format: 'iife',
  target: ['es2018'],
  outfile: 'build/userscript.ots.user.js',
  banner: {
    js: `// ==UserScript==\n// @name         OTS Game Dashboard Bridge\n// @namespace    http://tampermonkey.net/\n// @version      0.1.0\n// @description  Send game state and events to local Nuxt dashboard\n// @author       [PUSH] DUCKDUCK\n// @match        https://openfront.io/*\n// @grant        GM_getValue\n// @grant        GM_setValue\n// ==/UserScript==\n`
  }
})

console.log('Built userscript to ./build/userscript.ots.user.js')
