import esbuild from 'esbuild'
import { readFileSync } from 'fs'

// Read version from package.json
const packageJson = JSON.parse(readFileSync('./package.json', 'utf-8'))
const version = packageJson.version

await esbuild.build({
  entryPoints: ['src/main.user.ts'],
  bundle: true,
  format: 'iife',
  target: ['es2018'],
  outfile: 'build/userscript.ots.user.js',
  banner: {
    js: `// ==UserScript==\n// @name         OTS Game Dashboard Bridge\n// @namespace    http://tampermonkey.net/\n// @version      ${version}\n// @description  Send game state and events to OTS controller\n// @author       [PUSH] DUCKDUCK\n// @author       DeloVan\n// @author       [PUSH] Nono\n// @author       [PUSH] Rime\n// @match        https://openfront.io/*\n// @grant        GM_getValue\n// @grant        GM_setValue\n// ==/UserScript==\n`
  },
  define: {
    'VERSION': `"${version}"`
  }
})

console.log(`Built userscript v${version} to ./build/userscript.ots.user.js`)
