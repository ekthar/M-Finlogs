import { NextResponse } from "next/server";
import type { NextRequest } from "next/server";

/**
 * Middleware — protects BOTH pages AND API routes.
 * 
 * Public paths (no auth needed):
 * - /login page
 * - /api/auth/* (login/logout endpoints)
 * - /api/events (SSE for real-time, uses own auth)
 * 
 * Everything else requires a valid session token cookie.
 */

const publicPaths = ["/login", "/api/auth", "/api/events"];

export function middleware(request: NextRequest) {
  const { pathname } = request.nextUrl;

  // Allow public paths
  if (publicPaths.some((p) => pathname.startsWith(p))) {
    return NextResponse.next();
  }

  // Check for auth token/session cookie
  const token =
    request.cookies.get("next-auth.session-token")?.value ||
    request.cookies.get("__Secure-next-auth.session-token")?.value;

  if (!token) {
    // API routes: return 401 JSON
    if (pathname.startsWith("/api/")) {
      return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
    }
    // Pages: redirect to login
    const loginUrl = new URL("/login", request.url);
    return NextResponse.redirect(loginUrl);
  }

  // Add security headers
  const response = NextResponse.next();
  response.headers.set("X-Content-Type-Options", "nosniff");
  response.headers.set("X-Frame-Options", "DENY");
  response.headers.set("Referrer-Policy", "strict-origin-when-cross-origin");

  return response;
}

export const config = {
  matcher: [
    "/((?!_next/static|_next/image|favicon.ico|favicon.svg|.*\\..*$).*)",
  ],
};
