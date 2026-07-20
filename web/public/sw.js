/**
 * M-Finlogs Service Worker
 * 
 * Strategy: Network-first for API, Cache-first for static assets.
 * - Static assets (JS/CSS/fonts): cache on first load, serve from cache
 * - API calls: try network first, fall back to cached response
 * - Offline: show cached pages, queue mutations for later sync
 */

const CACHE_NAME = "mfinlogs-v1";
const STATIC_CACHE = "mfinlogs-static-v1";

const STATIC_URLS = [
  "/",
  "/login",
  "/daily-entry",
  "/manifest.json",
];

// Install: pre-cache critical pages
self.addEventListener("install", (event) => {
  event.waitUntil(
    caches.open(STATIC_CACHE).then((cache) => cache.addAll(STATIC_URLS))
  );
  self.skipWaiting();
});

// Activate: clean old caches
self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(keys.filter((k) => k !== CACHE_NAME && k !== STATIC_CACHE).map((k) => caches.delete(k)))
    )
  );
  self.clients.claim();
});

// Fetch: network-first for API, cache-first for assets
self.addEventListener("fetch", (event) => {
  const { request } = event;
  const url = new URL(request.url);

  // Skip non-GET requests (mutations go through offline queue)
  if (request.method !== "GET") return;

  // API calls: network first, cache fallback
  if (url.pathname.startsWith("/api/")) {
    event.respondWith(
      fetch(request)
        .then((response) => {
          // Cache successful GET responses
          if (response.ok) {
            const clone = response.clone();
            caches.open(CACHE_NAME).then((cache) => cache.put(request, clone));
          }
          return response;
        })
        .catch(() => caches.match(request).then((cached) => cached || new Response(JSON.stringify({ error: "Offline" }), { status: 503, headers: { "Content-Type": "application/json" } })))
    );
    return;
  }

  // Static assets: cache first, network fallback
  event.respondWith(
    caches.match(request).then((cached) => {
      if (cached) return cached;
      return fetch(request).then((response) => {
        if (response.ok && (url.pathname.endsWith(".js") || url.pathname.endsWith(".css") || url.pathname.endsWith(".woff2"))) {
          const clone = response.clone();
          caches.open(STATIC_CACHE).then((cache) => cache.put(request, clone));
        }
        return response;
      }).catch(() => caches.match("/") || new Response("Offline", { status: 503 }));
    })
  );
});
