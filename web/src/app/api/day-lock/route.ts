import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAdmin } from "@/lib/auth-guard";

/**
 * Day Locking — prevents edits to transactions on or before a locked date.
 * GET: fetch the current lock date
 * POST: set the lock date (admin only)
 */
export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";

    const setting = await prisma.appSetting.findFirst({
      where: { settingKey: "day_lock_date", companyId },
    });

    return NextResponse.json({ lockDate: setting?.settingValue || null });
  } catch (error) {
    console.error("GET /api/day-lock error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}

export async function POST(request: Request) {
  const auth = await requireAdmin(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { companyId = "cm_default_001", lockDate } = await request.json();

    if (!lockDate) {
      return NextResponse.json({ error: "lockDate required (YYYY-MM-DD)" }, { status: 400 });
    }

    const settingKey = "day_lock_date";
    const existing = await prisma.appSetting.findFirst({
      where: { settingKey, companyId },
    });

    if (existing) {
      await prisma.appSetting.update({
        where: { id: existing.id },
        data: { settingValue: lockDate },
      });
    } else {
      await prisma.appSetting.create({
        data: { settingKey, settingValue: lockDate, companyId },
      });
    }

    return NextResponse.json({ status: "Locked", lockDate });
  } catch (error) {
    console.error("POST /api/day-lock error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
