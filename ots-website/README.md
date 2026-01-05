# OTS Website

Static documentation website for the OpenFront Tactical Suitcase, built with VitePress.

## Setup

```bash
npm install
```

## Development

```bash
npm run dev
```

Visit http://localhost:5173

## Build

```bash
npm run build
```

Output: `.vitepress/dist/`

## Preview Production Build

```bash
npm run preview
```

## How it Works

- **Source**: Reads markdown files from `../doc/` (the main repo's doc folder)
- **Config**: `.vitepress/config.ts` defines navigation and theme
- **Landing**: `index.md` is the custom homepage (not from doc/)
- **Output**: Static HTML generated to `.vitepress/dist/`

## Deployment

To deploy to GitHub Pages, see `.github/workflows/deploy.yml` (TODO).

The site will be available at: `https://yourusername.github.io/ots/`
