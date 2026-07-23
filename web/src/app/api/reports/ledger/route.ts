import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAuth } from "@/lib/auth-guard";

export async function GET(request: Request) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { searchParams } = new URL(request.url);
    const partyName = searchParams.get("party") || "";
    const startDate = searchParams.get("start") || "";
    const endDate = searchParams.get("end") || "";
    const companyId = searchParams.get("companyId") || "";

    if (!partyName) {
      return NextResponse.json({ error: "Party name is required" }, { status: 400 });
    }

    const normalized = partyName.trim().toLowerCase().replace(/\s+/g, "_");
    const party = await prisma.party.findFirst({
      where: { normalizedName: normalized, ...(companyId ? { companyId } : {}) },
    });

    if (!party) {
      return NextResponse.json({ error: "Party not found" }, { status: 404 });
    }

    // ── Build date filter ──
    // Supports: both dates, only start, only end, or no dates (show ALL)
    const dateFilter: Record<string, unknown> = {};
    if (startDate) {
      dateFilter.gte = new Date(startDate);
    }
    if (endDate) {
      // End date should be inclusive (end of day)
      const end = new Date(endDate);
      end.setHours(23, 59, 59, 999);
      dateFilter.lte = end;
    }

    const where: Record<string, unknown> = { partyId: party.partyId };
    if (Object.keys(dateFilter).length > 0) {
      where.txnDate = dateFilter;
    }

    // ── NO PAGINATION — return ALL transactions for this party ──
    // Ledger must show complete history for running balance to be correct
    const transactions = await prisma.transaction.findMany({
      where,
      orderBy: [{ txnDate: "asc" }, { txnId: "asc" }],
    });

    // ── Calculate opening balance ──
    // 1. Party's stored opening balance (from FY carry-forward)
    let openingBalance = 0;
    const obSetting = await prisma.appSetting.findFirst({
      where: { companyId: companyId || undefined, settingKey: `opening_balance:${party.partyId}` },
    });
    if (obSetting) {
      openingBalance = parseFloat(obSetting.settingValue || "0");
    }

    // 2. Add transactions BEFORE the start date (if date-filtered)
    if (startDate) {
      const priorTransactions = await prisma.transaction.findMany({
        where: {
          partyId: party.partyId,
          txnDate: { lt: new Date(startDate) },
        },
        select: { txnType: true, amount: true },
      });

      for (const t of priorTransactions) {
        const amt = Number(t.amount);
        if (t.txnType === "Sale" || t.txnType === "Expense" || t.txnType === "Purchase") {
          openingBalance += amt;
        } else {
          openingBalance -= amt;
        }
      }
    }

    // ── Build ledger with running balance ──
    let balance = openingBalance;
    const ledger = transactions.map((t) => {
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
        txnId: t.txnId,
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
      ledger,
      totalBalance: balance,
      openingBalance,
      totalCount: transactions.length,
    });
  } catch (error) {
    console.error("GET /api/reports/ledger error:", error);
    return NextResponse.json({ error: "Failed to fetch ledger" }, { status: 500 });
  }
}
