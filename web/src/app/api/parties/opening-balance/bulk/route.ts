import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAdmin } from "@/lib/auth-guard";

/**
 * Bulk import opening balances for all parties.
 * Body: { companyId, balances: [{ partyId: number, balance: number }] }
 */
export async function POST(request: Request) {
  const auth = await requireAdmin(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { companyId = "cm_default_001", balances } = await request.json();

    if (!Array.isArray(balances) || balances.length === 0) {
      return NextResponse.json({ error: "balances array required" }, { status: 400 });
    }

    let updated = 0;
    for (const { partyId, balance } of balances) {
      if (!partyId || balance === undefined) continue;

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
      updated++;
    }

    return NextResponse.json({ status: "Imported", updated });
  } catch (error) {
    console.error("POST /api/parties/opening-balance/bulk error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
