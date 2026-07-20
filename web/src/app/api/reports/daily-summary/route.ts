import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const startDate = searchParams.get("start") || "";
    const endDate = searchParams.get("end") || "";
    const companyId = searchParams.get("companyId") || "";

    const end = endDate ? new Date(endDate) : new Date();
    const start = startDate ? new Date(startDate) : new Date(end.getTime() - 29 * 86400000);

    const where: Record<string, unknown> = {
      txnDate: { gte: start, lte: end },
    };
    if (companyId) where.companyId = companyId;

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
      } else if (t.txnType === "Expense" || t.txnType === "Sale Return") {
        if (t.paymentMode === "Cash") byDate[key].cashExp += amt;
      }
    }

    // Get opening cash values
    const cashRecords = await prisma.dailyCash.findMany({
      where: { cashDate: { gte: start, lte: end }, ...(companyId ? { companyId } : {}) },
    });
    const cashMap: Record<string, number> = {};
    for (const c of cashRecords) {
      cashMap[new Date(c.cashDate).toISOString().split("T")[0]] = Number(c.cashInHand);
    }

    const dates = Object.keys(byDate).sort();
    let runningCash = 0;

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
