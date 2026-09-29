// Hosting build (`npm run online:build`): the game's code into web/dist-online without copying web/public (the 1.8 GB of
// extracted game data stays where it is; web/server/mp-server.mjs serves dist-online first, then public).
import base from '../vite.config.js';
export default { ...base, build: { ...base.build, outDir: 'dist-online', emptyOutDir: true, copyPublicDir: false, rolldownOptions: { ...base.build.rolldownOptions, input: { main: 'index.html' } } } };
