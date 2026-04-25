/**
 * GreenHouse IQ - Service Worker
 * Enables offline use of the dashboard
 */

const CACHE_NAME = 'greenhouse-iq-v1';
const ASSETS = [
  '/',
  '/index.html',
  '/src/bleManager.js',
  '/src/bleConstants.js',
  '/manifest.json',
  'https://fonts.googleapis.com/css2?family=Space+Mono:ital,wght@0,400;0,700;1,400&family=Syne:wght@400;600;700;800&display=swap',
];

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME).then((cache) => {
      console.log('[SW] Caching app shell');
      return cache.addAll(ASSETS.filter(a => !a.startsWith('https://fonts')));
    })
  );
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(keys.filter(k => k !== CACHE_NAME).map(k => caches.delete(k)))
    )
  );
  self.clients.claim();
});

self.addEventListener('fetch', (event) => {
  // Network-first for API calls, cache-first for assets
  const url = new URL(event.request.url);

  if (url.pathname.startsWith('/api/')) {
    // Network first
    event.respondWith(
      fetch(event.request).catch(() =>
        caches.match(event.request)
      )
    );
    return;
  }

  // Cache first with network fallback
  event.respondWith(
    caches.match(event.request).then((cached) => {
      if (cached) return cached;
      return fetch(event.request).then((response) => {
        if (response.ok) {
          const clone = response.clone();
          caches.open(CACHE_NAME).then((cache) => cache.put(event.request, clone));
        }
        return response;
      }).catch(() => {
        // Fallback to index.html for navigation
        if (event.request.mode === 'navigate') {
          return caches.match('/index.html');
        }
      });
    })
  );
});
