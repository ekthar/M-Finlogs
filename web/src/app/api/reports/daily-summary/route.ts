import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAuth } from "@/lib/auth-guard";

/**
 * Daily Summary — Fixed accounting logic:
 * - Opening cash calculated from last known cashInHand BEFORE the date range
 * - Purchase expenses counted as cash out
 * - FY filter support
 */
export async function GET(request: Request) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { searchParams } = new URL(request.url);
    const startDate = searchParams.get("start") || "";
    const endDate = searchParams.get("end") || "";
    const companyId = searchParams.get("companyId") || "";
    const financialYear = searchParams.get("financialYear") || "";

    const end = endDate ? new Date(endDate) : new Date();
    const start = startDate ? new Date(startDate) : new Date(end.getTime() - 29 * 86400000);

    const where: Record<string, unknown> = {
      txnDate: { gte: start, lte: end },
    };
    if (companyId) where.companyId = companyId;
    if (financialYear) where.financialYear = financialYear;

    const transactions = await prisma.transaction.findMany({ where, orderBy: { txnDate: "asc" } });

    // Group by date
    const byDate: Record<string, { cashIn: number; cashExp: number; bank: number; credit: number; totalSales: number }> = {};

    for (const t of transactions) {
      const key = new Date(t.txnDate).toISOString().split("T")[0];
      if (!byDate[key]) byDate[key] = { cashIn: 0, cashExp: 0, bank: 0, credit: 0, totalSales: 0 };
      const amt = Number(t.amount);

      if (t.txnType === "Sale" || t.txnType === "Receipt") {
        if (t.paymentMode === "Cash") byDate[key].cashIn += amt;
        else if (t.paymentMode === "Bank" || t.paymentMode === "UPI") byDate[key].bank += amt;
        else if (t.paymentMode === "Credit") byDate[key].credit += amt;
        byDate[key].totalSales += amt;
      } else if (t.txnType === "Expense" || t.txnType === "Sale Return" || t.txnType === "Purchase") {
        if (t.paymentMode === "Cash") byDate[key].cashExp += amt;
      }
    }

    // Get opening cash values (manual entries)
    const cashRecords = await prisma.dailyCash.findMany({
      where: { cashDate: { gte: start, lte: end }, ...(companyId ? { companyId } : {}) },
    });
    const cashMap: Record<string, number> = {};
    for (const c of cashRecords) {
      cashMap[new Date(c.cashDate).toISOString().split("T")[0]] = Number(c.cashInHand);
    }

    // ── FIX #5: Get proper opening cash from BEFORE the start date ──
    let runningCash = 0;

    // Try to get the last recorded cashInHand before start date
    const lastCashRecord = await prisma.dailyCash.findFirst({
      where: {
        cashDate: { lt: start },
        ...(companyId ? { companyId } : {}),
      },
      orderBy: { cashDate: "desc" },
    });

    if (lastCashRecord) {
      runningCash = Number(lastCashRecord.cashInHand);
    } else {
      // Calculate from all prior cash transactions
      const priorWhere: Record<string, unknown> = {
        txnDate: { lt: start },
      };
      if (companyId) priorWhere.companyId = companyId;

      const priorTransactions = await prisma.transaction.findMany({
        where: priorWhere,
        select: { txnType: true, paymentMode: true, amount: true },
      });

      for (const t of priorTransactions) {
        const amt = Number(t.amount);
        if ((t.txnType === "Sale" || t.txnType === "Receipt") && t.paymentMode === "Cash") {
          runningCash += amt;
        } else if ((t.txnType === "Expense" || t.txnType === "Sale Return" || t.txnType === "Purchase") && t.paymentMode === "Cash") {
          runningCash -= amt;
        }
      }
    }

    const dates = Object.keys(byDate).sort();

    const summary = dates.map((date) => {
      const d = byDate[date];
      const openingCash = runningCash;
      const cashNeeded = openingCash + d.cashIn - d.cashExp;
      const cashInHand = cashMap[date] !== undefined ? cashMap[date] : cashNeeded;
      const shortExcess = cashInHand - cashNeeded;
      runningCash = cashInHand;

      return {
        date,
        openingCash,
        cashIn: d.cashIn,
        cashExp: d.cashExp,
        cashNeeded,
        cashInHand,
        shortExcess,
        bank: d.bank,
        credit: d.credit,
        totalSales: d.totalSales,
      };
    });

    return NextResponse.json({ summary });
  } catch (error) {
    console.error("GET /api/reports/daily-summary error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
