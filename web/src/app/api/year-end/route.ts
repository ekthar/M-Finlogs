import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAdmin } from "@/lib/auth-guard";

/**
 * Year-End Close Process (#47)
 * ────────────────────────────
 * Carries forward all party balances from the closing FY to the new FY.
 *
 * Process:
 * 1. Calculate closing balance for every party in the specified FY
 * 2. Store as opening_balance for the next FY in AppSettings
 * 3. Log the year-end close in audit
 *
 * POST body: { companyId, closingFY: "2025-2026" }
 * Result: { partiesProcessed, balancesCarriedForward, totalDebit, totalCredit }
 */
export async function POST(request: Request) {
  const auth = await requireAdmin(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { companyId = "cm_default_001", closingFY } = await request.json();

    if (!closingFY || !/^\d{4}-\d{4}$/.test(closingFY)) {
      return NextResponse.json({ error: "closingFY required (format: YYYY-YYYY)" }, { status: 400 });
    }

    const [startYear, endYear] = closingFY.split("-").map(Number);
    if (endYear !== startYear + 1) {
      return NextResponse.json({ error: "Invalid FY (must be consecutive years)" }, { status: 400 });
    }

    const nextFY = `${endYear}-${endYear + 1}`;

    // ── Get all parties ──
    const parties = await prisma.party.findMany({
      where: { companyId },
      select: { partyId: true, name: true },
    });

    let partiesProcessed = 0;
    let balancesCarriedForward = 0;
    let totalDebit = 0;
    let totalCredit = 0;
    const results: { party: string; balance: number }[] = [];

    for (const party of parties) {
      // Get existing opening balance for closing FY
      let openingBal = 0;
      const obSetting = await prisma.appSetting.findFirst({
        where: { companyId, settingKey: `opening_balance:${party.partyId}` },
      });
      if (obSetting) {
        openingBal = parseFloat(obSetting.settingValue || "0");
      }

      // Calculate balance from transactions in this FY
      const transactions = await prisma.transaction.findMany({
        where: { partyId: party.partyId, financialYear: closingFY, companyId },
        select: { txnType: true, amount: true },
      });

      let fyBalance = openingBal;
      for (const t of transactions) {
        const amt = Number(t.amount);
        if (t.txnType === "Sale" || t.txnType === "Expense" || t.txnType === "Purchase") {
          fyBalance += amt;
        } else {
          fyBalance -= amt;
        }
      }

      // Only carry forward non-zero balances
      if (Math.abs(fyBalance) >= 0.01) {
        // Store as opening balance for next FY
        const settingKey = `opening_balance:${party.partyId}`;
        await prisma.appSetting.upsert({
          where: { settingKey_companyId: { settingKey, companyId } },
          update: { settingValue: String(fyBalance) },
          create: { settingKey, settingValue: String(fyBalance), companyId },
        });

        balancesCarriedForward++;
        if (fyBalance > 0) totalDebit += fyBalance;
        else totalCredit += Math.abs(fyBalance);
        results.push({ party: party.name, balance: fyBalance });
      }

      partiesProcessed++;
    }

    // ── Audit log ──
    try {
      const username = (auth as any).username || "admin";
      await prisma.auditLog.create({
        data: {
          username,
          action: "YEAR_END_CLOSE",
          details: `Closed FY ${closingFY} → ${nextFY}. ${balancesCarriedForward} balances carried forward. Debit: ₹${totalDebit.toFixed(2)}, Credit: ₹${totalCredit.toFixed(2)}`,
          companyId,
        },
      });
    } catch {}

    return NextResponse.json({
      status: "Year-end close complete",
      closingFY,
      nextFY,
      partiesProcessed,
      balancesCarriedForward,
      totalDebit,
      totalCredit,
      results: results.sort((a, b) => Math.abs(b.balance) - Math.abs(a.balance)),
    });
  } catch (error) {
    console.error("POST /api/year-end error:", error);
    return NextResponse.json({ error: "Year-end close failed" }, { status: 500 });
  }
}
