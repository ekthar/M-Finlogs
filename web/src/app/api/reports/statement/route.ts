import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAuth } from "@/lib/auth-guard";

/**
 * Party Statement API
 * ───────────────────
 * Generates a complete account statement for a party:
 * - Opening balance (from AppSettings + prior transactions)
 * - All transactions in date range
 * - Running balance per entry
 * - Closing balance
 *
 * Used by: exportPartyStatement() in export-pdf.ts
 * Query: ?party=Name&companyId=X&start=YYYY-MM-DD&end=YYYY-MM-DD
 */
export async function GET(request: Request) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { searchParams } = new URL(request.url);
    const partyName = searchParams.get("party") || "";
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const startDate = searchParams.get("start") || "";
    const endDate = searchParams.get("end") || "";

    if (!partyName) {
      return NextResponse.json({ error: "Party name is required" }, { status: 400 });
    }

    const normalized = partyName.trim().toLowerCase().replace(/\s+/g, "_");
    const party = await prisma.party.findFirst({
      where: { normalizedName: normalized, companyId },
    });

    if (!party) {
      return NextResponse.json({ error: "Party not found" }, { status: 404 });
    }

    // ── Opening balance from AppSettings ──
    let openingBalance = 0;
    const obSetting = await prisma.appSetting.findFirst({
      where: { companyId, settingKey: `opening_balance:${party.partyId}` },
    });
    if (obSetting) {
      openingBalance = parseFloat(obSetting.settingValue || "0");
    }

    // ── Prior transactions (before start date) ──
    if (startDate) {
      const priorTxns = await prisma.transaction.findMany({
        where: { partyId: party.partyId, txnDate: { lt: new Date(startDate) } },
        select: { txnType: true, amount: true },
      });
      for (const t of priorTxns) {
        const amt = Number(t.amount);
        if (t.txnType === "Sale" || t.txnType === "Expense" || t.txnType === "Purchase") {
          openingBalance += amt;
        } else {
          openingBalance -= amt;
        }
      }
    }

    // ── Transactions in range ──
    const dateFilter: Record<string, unknown> = {};
    if (startDate) dateFilter.gte = new Date(startDate);
    if (endDate) {
      const end = new Date(endDate);
      end.setHours(23, 59, 59, 999);
      dateFilter.lte = end;
    }

    const where: Record<string, unknown> = { partyId: party.partyId };
    if (Object.keys(dateFilter).length > 0) where.txnDate = dateFilter;

    const transactions = await prisma.transaction.findMany({
      where,
      orderBy: [{ txnDate: "asc" }, { txnId: "asc" }],
    });

    // ── Build statement with running balance ──
    let balance = openingBalance;
    const entries = transactions.map((t) => {
      const amt = Number(t.amount);
      let debit = 0;
      let credit = 0;

      if (t.txnType === "Sale" || t.txnType === "Expense" || t.txnType === "Purchase") {
        debit = amt;
        balance += amt;
      } else {
        credit = amt;
        balance -= amt;
      }

      return {
        date: t.txnDate,
        billNo: t.billNo,
        txnType: t.txnType,
        paymentMode: t.paymentMode,
        debit,
        credit,
        balance,
      };
    });

    return NextResponse.json({
      party: party.name,
      partyType: party.type,
      openingBalance,
      closingBalance: balance,
      totalDebit: entries.reduce((s, e) => s + e.debit, 0),
      totalCredit: entries.reduce((s, e) => s + e.credit, 0),
      entries,
      entryCount: entries.length,
      dateRange: {
        from: startDate || (transactions[0] ? new Date(transactions[0].txnDate).toISOString().split("T")[0] : ""),
        to: endDate || (transactions.length > 0 ? new Date(transactions[transactions.length - 1].txnDate).toISOString().split("T")[0] : ""),
      },
    });
  } catch (error) {
    console.error("GET /api/reports/statement error:", error);
    return NextResponse.json({ error: "Failed to generate statement" }, { status: 500 });
  }
}
