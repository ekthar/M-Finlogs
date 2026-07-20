import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";

/**
 * Multi-Branch Support.
 * Branches are stored as app_settings with key "branches".
 * Each branch has: id, name, location.
 * Transactions can be filtered by branch (stored in bill_no prefix or separate field).
 * 
 * For now: branch list management. Filtering is done client-side via AppContext.
 */
export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";

    const setting = await prisma.appSetting.findFirst({
      where: { settingKey: "branches", companyId },
    });

    let branches = [{ id: "main", name: "Main Branch", location: "Default" }];
    if (setting?.settingValue) {
      try { branches = JSON.parse(setting.settingValue); } catch {}
    }

    return NextResponse.json({ branches });
  } catch (error) {
    console.error("GET /api/branches error:", error);
    return NextResponse.json({ branches: [{ id: "main", name: "Main", location: "" }] });
  }
}

export async function POST(request: Request) {
  try {
    const { companyId = "cm_default_001", branches } = await request.json();

    if (!Array.isArray(branches)) {
      return NextResponse.json({ error: "branches array required" }, { status: 400 });
    }

    const settingKey = "branches";
    const existing = await prisma.appSetting.findFirst({
      where: { settingKey, companyId },
    });

    const value = JSON.stringify(branches);
    if (existing) {
      await prisma.appSetting.update({ where: { id: existing.id }, data: { settingValue: value } });
    } else {
      await prisma.appSetting.create({ data: { settingKey, settingValue: value, companyId } });
    }

    return NextResponse.json({ status: "Saved", branches });
  } catch (error) {
    console.error("POST /api/branches error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
