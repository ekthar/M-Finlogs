import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";

/**
 * Opening balance for parties — stores as an app_setting keyed by party_id.
 * GET: fetch opening balance for a party
 * POST: set opening balance for a party
 */
export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const partyId = searchParams.get("partyId") || "";

    if (!partyId) {
      // Return all opening balances
      const settings = await prisma.appSetting.findMany({
        where: { companyId, settingKey: { startsWith: "opening_balance:" } },
      });
      const balances: Record<string, number> = {};
      for (const s of settings) {
        const pid = s.settingKey.replace("opening_balance:", "");
        balances[pid] = parseFloat(s.settingValue || "0");
      }
      return NextResponse.json({ balances });
    }

    const setting = await prisma.appSetting.findFirst({
      where: { companyId, settingKey: `opening_balance:${partyId}` },
    });

    return NextResponse.json({ partyId, balance: parseFloat(setting?.settingValue || "0") });
  } catch (error) {
    console.error("GET /api/parties/opening-balance error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}

export async function POST(request: Request) {
  try {
    const { companyId = "cm_default_001", partyId, balance } = await request.json();

    if (!partyId || balance === undefined) {
      return NextResponse.json({ error: "partyId and balance required" }, { status: 400 });
    }

    const settingKey = `opening_balance:${partyId}`;
    const existing = await prisma.appSetting.findFirst({
      where: { settingKey, companyId },
    });

    if (existing) {
      await prisma.appSetting.update({
        where: { id: existing.id },
        data: { settingValue: String(balance) },
      });
    } else {
      await prisma.appSetting.create({
        data: { settingKey, settingValue: String(balance), companyId },
      });
    }

    return NextResponse.json({ status: "Saved", partyId, balance });
  } catch (error) {
    console.error("POST /api/parties/opening-balance error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
