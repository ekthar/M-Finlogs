"use client";

import { useState, useEffect, useCallback, useRef } from "react";

interface CacheEntry<T> {
  data: T;
  timestamp: number;
}

const memoryCache = new Map<string, CacheEntry<unknown>>();
const STALE_TIME = 30000; // 30s — show cached, refresh in background

/**
 * Cached fetch hook — shows stale data instantly, refreshes in background.
 * 
 * Strategy (stale-while-revalidate):
 * 1. If cached data exists and is < 30s old → return immediately, no fetch
 * 2. If cached data exists but stale → return immediately + fetch in background
 * 3. If no cache → fetch and show loading
 */
export function useCachedFetch<T>(url: string | null, initialData?: T) {
  const [data, setData] = useState<T | undefined>(initialData);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const fetchingRef = useRef(false);

  const fetchData = useCallback(async (showLoading = true) => {
    if (!url || fetchingRef.current) return;
    fetchingRef.current = true;

    // Check memory cache
    const cached = memoryCache.get(url) as CacheEntry<T> | undefined;
    if (cached) {
      setData(cached.data);
      setLoading(false);
      // If fresh enough, skip refetch
      if (Date.now() - cached.timestamp < STALE_TIME) {
        fetchingRef.current = false;
        return;
      }
    } else if (showLoading) {
      setLoading(true);
    }

    try {
      const res = await fetch(url);
      if (!res.ok) throw new Error(`${res.status}`);
      const json = await res.json();
      setData(json);
      setError(null);
      memoryCache.set(url, { data: json, timestamp: Date.now() });
    } catch (e) {
      setError(String(e));
      // Don't clear stale data on error — keep showing what we have
    } finally {
      setLoading(false);
      fetchingRef.current = false;
    }
  }, [url]);

  useEffect(() => { fetchData(); }, [fetchData]);

  const refresh = useCallback(() => {
    memoryCache.delete(url || "");
    return fetchData(false);
  }, [url, fetchData]);

  const invalidate = useCallback(() => {
    memoryCache.delete(url || "");
  }, [url]);

  return { data, loading: loading && !data, error, refresh, invalidate };
}

/**
 * Invalidate all cache entries matching a prefix
 */
export function invalidateCache(prefix: string) {
  for (const key of memoryCache.keys()) {
    if (key.includes(prefix)) memoryCache.delete(key);
  }
}
