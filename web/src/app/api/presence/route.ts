import { NextResponse } from "next/server";

/**
 * Presence system — tracks who's currently online.
 * Uses in-memory store (resets on server restart, fine for serverless with low traffic).
 * 
 * POST: user heartbeat (every 10s from client)
 * GET: list all online users (active within last 30s)
 */

interface PresenceEntry {
  username: string;
  page: string;
  lastSeen: number;
}

// In-memory store (per serverless instance — adequate for 2-10 users)
const presenceMap = new Map<string, PresenceEntry>();
const ONLINE_THRESHOLD = 30000; // 30 seconds

export async function POST(request: Request) {
  try {
    const { username, page } = await request.json();
    if (!username) return NextResponse.json({ error: "username required" }, { status: 400 });

    presenceMap.set(username, { username, page: page || "/", lastSeen: Date.now() });

    // Clean stale entries
    const now = Date.now();
    for (const [key, entry] of presenceMap.entries()) {
      if (now - entry.lastSeen > ONLINE_THRESHOLD * 2) presenceMap.delete(key);
    }

    return NextResponse.json({ status: "ok" });
  } catch {
    return NextResponse.json({ status: "ok" });
  }
}

export async function GET() {
  const now = Date.now();
  const online: PresenceEntry[] = [];

  for (const entry of presenceMap.values()) {
    if (now - entry.lastSeen < ONLINE_THRESHOLD) {
      online.push(entry);
    }
  }

  return NextResponse.json({ online, count: online.length });
}
