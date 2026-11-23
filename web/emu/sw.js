/* SPDX-License-Identifier: MPL-2.0 */
/* The portable instrument owns one versioned cache within its /emu/ scope. */
const CACHE_NAME = 'aura-studio-v1';
const PRECACHE_ASSETS = [
  './','index.html','manual.html','worklet.js','app.js','app_worklet.js',
  'app.wasm','amb.wasm','manifest.webmanifest',
  'favicon.ico','icon.svg','icon-192.png','icon-512.png','reader/words.js','reader/tts.js',
  'assets/studio.css','assets/studio-emu.css','assets/studio-controls.js',
  'assets/site.js',
  'assets/fonts/manrope-400_800-normal.woff2','assets/icon.svg','assets/favicon.ico','assets/apple-touch-icon.png',
  'img/fm1-panel.svg','img/screen-garden.png','img/screen-voices.png',
  'img/screen-help.png','img/screen-turning.png','img/screen-pad.png',
  'img/screen-bells.png','img/screen-bass.png','img/screen-pulse.png',
  'img/screen-space.png','img/screen-setup.png',
  'img/screen-frozen.png','img/screen-cosmic.png',
  'img/screen-texture.png','img/screen-solo.png',
  /* WORD_FILES */
];
self.addEventListener('install',event=>{
  self.skipWaiting();
  event.waitUntil(caches.open(CACHE_NAME).then(cache=>Promise.all(PRECACHE_ASSETS.map(async url=>{
    try{const response=await fetch(url,{cache:'reload'});if(response.ok)await cache.put(url,response);}catch(_){}
  }))));
});
self.addEventListener('activate',event=>{
  event.waitUntil((async()=>{
    for(const key of await caches.keys())if(key!==CACHE_NAME&&/^(aura-|amb-)/.test(key))await caches.delete(key);
    await self.clients.claim();
    for(const client of await self.clients.matchAll())client.postMessage({type:'SW_UPDATED',version:CACHE_NAME});
  })());
});
self.addEventListener('fetch',event=>{
  const request=event.request;
  if(request.method!=='GET'||new URL(request.url).origin!==self.location.origin)return;
  if(request.mode==='navigate'){
    event.respondWith((async()=>{
      const cache=await caches.open(CACHE_NAME);
      try{const response=await fetch(request);if(response.ok)await cache.put(request,response.clone());return response;}
      catch(_){return await cache.match(request)||await cache.match(new URL(request.url).pathname.endsWith('manual.html')?'manual.html':'index.html')||Response.error();}
    })());
    return;
  }
  event.respondWith((async()=>{
    const cache=await caches.open(CACHE_NAME),cached=await cache.match(request);
    if(cached)return cached;
    try{const response=await fetch(request);if(response.ok)await cache.put(request,response.clone());return response;}
    catch(_){return Response.error();}
  })());
});
self.addEventListener('message',event=>{if(event.data?.type==='SKIP_WAITING')self.skipWaiting();});
