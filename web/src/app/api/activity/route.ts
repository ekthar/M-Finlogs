import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";

/**
 * Activity Feed — returns recent actions (last 20 audit log entries).
 * Real-time updates via polling (every 10s from frontend).
 * For true WebSocket: use Supabase Realtime subscription on client.
 */
export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const limit = Math.min(parseInt(searchParams.get("limit") || "20"), 50);

    const activities = await prisma.auditLog.findMany({
      where: { companyId },
      orderBy: { timestamp: "desc" },
      take: limit,
      select: { id: true, timestamp: true, username: true, action: true, details: true },
    });

    return NextResponse.json({
      activities: activities.map(a => ({
        id: a.id,
        time: a.timestamp,
        user: a.username,
        action: a.action,
        details: a.details,
      })),
    });
  } catch (error) {
    console.error("GET /api/activity error:", error);
    return NextResponse.json({ activities: [] });
  }
}
