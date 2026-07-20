import { NextResponse } from "next/server";
import { jwtVerify } from "jose";
import { cookies } from "next/headers";

const secret = new TextEncoder().encode(process.env.JWT_SECRET || "fallback-secret-change-me");

export interface AuthPayload {
  username: string;
  role: string;
}

/**
 * Verify JWT from cookie or Authorization header.
 * Use in API routes: const auth = await verifyAuth(request);
 * Returns null if unauthorized.
 */
export async function verifyAuth(request: Request): Promise<AuthPayload | null> {
  try {
    // Check Authorization header first
    const authHeader = request.headers.get("authorization");
    let token = "";

    if (authHeader?.startsWith("Bearer ")) {
      token = authHeader.slice(7);
    } else {
      // Fallback to cookie
      const cookieStore = await cookies();
      token = cookieStore.get("next-auth.session-token")?.value || "";
    }

    if (!token) return null;

    const { payload } = await jwtVerify(token, secret);
    return {
      username: payload.username as string,
      role: payload.role as string,
    };
  } catch {
    return null;
  }
}

/**
 * Require authentication — returns 401 response if not authenticated
 */
export async function requireAuth(request: Request): Promise<AuthPayload | NextResponse> {
  const auth = await verifyAuth(request);
  if (!auth) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }
  return auth;
}

/**
 * Require admin role — returns 403 response if not admin
 */
export async function requireAdmin(request: Request): Promise<AuthPayload | NextResponse> {
  const auth = await verifyAuth(request);
  if (!auth) return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  if (auth.role !== "admin") return NextResponse.json({ error: "Admin access required" }, { status: 403 });
  return auth;
}

// Rate limiting for login (in-memory, per-instance)
const loginAttempts = new Map<string, { count: number; lastAttempt: number }>();
const MAX_LOGIN_ATTEMPTS = 5;
const LOCKOUT_DURATION = 15 * 60 * 1000; // 15 minutes

export function checkRateLimit(ip: string): { allowed: boolean; retryAfter?: number } {
  const now = Date.now();
  const record = loginAttempts.get(ip);

  if (!record) return { allowed: true };

  // Reset if lockout period passed
  if (now - record.lastAttempt > LOCKOUT_DURATION) {
    loginAttempts.delete(ip);
    return { allowed: true };
  }

  if (record.count >= MAX_LOGIN_ATTEMPTS) {
    const retryAfter = Math.ceil((LOCKOUT_DURATION - (now - record.lastAttempt)) / 1000);
    return { allowed: false, retryAfter };
  }

  return { allowed: true };
}

export function recordLoginAttempt(ip: string, success: boolean) {
  if (success) {
    loginAttempts.delete(ip);
    return;
  }
  const record = loginAttempts.get(ip) || { count: 0, lastAttempt: 0 };
  record.count++;
  record.lastAttempt = Date.now();
  loginAttempts.set(ip, record);
}
