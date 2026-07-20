"use client";

/**
 * CSRF Protection — generates and validates tokens.
 * 
 * Strategy: Double-submit cookie pattern.
 * 1. On page load, generate a random CSRF token and store in cookie + memory
 * 2. On every mutation (POST/PUT/DELETE), include token in X-CSRF-Token header
 * 3. Server middleware compares header token with cookie token
 * 
 * For this app (same-origin API routes + httpOnly session cookie),
 * the primary protection is SameSite=Lax on the auth cookie.
 * This adds defense-in-depth.
 */

const TOKEN_KEY = "mfinlogs_csrf";

export function generateCsrfToken(): string {
  const token = crypto.randomUUID();
  if (typeof document !== "undefined") {
    document.cookie = `${TOKEN_KEY}=${token}; path=/; SameSite=Strict; max-age=86400`;
  }
  return token;
}

export function getCsrfToken(): string {
  if (typeof document === "undefined") return "";
  const match = document.cookie.match(new RegExp(`${TOKEN_KEY}=([^;]+)`));
  if (match) return match[1];
  return generateCsrfToken();
}

/**
 * Add CSRF token to fetch headers.
 * Usage: fetch(url, { headers: csrfHeaders() })
 */
export function csrfHeaders(): Record<string, string> {
  return { "X-CSRF-Token": getCsrfToken() };
}
