import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAuth } from "@/lib/auth-guard";

/**
 * Monthly Summary Report
 * Groups transactions by month, calculates:
 * - Total Sales, Expenses, Receipts per month
 * - Cash In, Cash Out, Bank, Credit
 * - Net profit (Sales - Expenses)
 */
export async function GET(request: Request) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const financialYear = searchParams.get("financialYear") || "";

    const where: Record<string, unknown> = {};
    if (companyId) where.companyId = companyId;
    if (financialYear) where.financialYear = financialYear;

    const transactions = await prisma.transaction.findMany({
      where,
      orderBy: { txnDate: "asc" },
    });

    // Group by month (YYYY-MM)
    const byMonth: Record<string, {
      sales: number; expenses: number; receipts: number; purchases: number;
      cashIn: number; cashOut: number; bank: number; credit: number;
      totalIn: number; totalOut: number; txnCount: number;
    }> = {};

    for (const t of transactions) {
      const d = new Date(t.txnDate);
      const key = `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}`;
      if (!byMonth[key]) {
        byMonth[key] = { sales: 0, expenses: 0, receipts: 0, purchases: 0, cashIn: 0, cashOut: 0, bank: 0, credit: 0, totalIn: 0, totalOut: 0, txnCount: 0 };
      }

      const amt = Number(t.amount);
      const m = byMonth[key];
      m.txnCount++;

      switch (t.txnType) {
        case "Sale":
          m.sales += amt;
          m.totalIn += amt;
          if (t.paymentMode === "Cash") m.cashIn += amt;
          else if (t.paymentMode === "Bank" || t.paymentMode === "UPI") m.bank += amt;
          else if (t.paymentMode === "Credit") m.credit += amt;
          break;
        case "Receipt":
          m.receipts += amt;
          m.totalIn += amt;
          if (t.paymentMode === "Cash") m.cashIn += amt;
          else if (t.paymentMode === "Bank" || t.paymentMode === "UPI") m.bank += amt;
          break;
        case "Expense":
          m.expenses += amt;
          m.totalOut += amt;
          if (t.paymentMode === "Cash") m.cashOut += amt;
          break;
        case "Sale Return":
          m.totalOut += amt;
          if (t.paymentMode === "Cash") m.cashOut += amt;
          break;
        case "Purchase":
          m.purchases += amt;
          m.totalOut += amt;
          if (t.paymentMode === "Cash") m.cashOut += amt;
          break;
      }
    }

    const months = Object.keys(byMonth).sort();
    const summary = months.map((month) => {
      const d = byMonth[month];
      return {
        month,
        label: new Date(month + "-01").toLocaleDateString("en-IN", { month: "long", year: "numeric" }),
        ...d,
        netProfit: d.sales - d.expenses,
        netCash: d.cashIn - d.cashOut,
      };
    });

    // Grand totals
    const totals = summary.reduce(
      (acc, m) => ({
        sales: acc.sales + m.sales,
        expenses: acc.expenses + m.expenses,
        receipts: acc.receipts + m.receipts,
        purchases: acc.purchases + m.purchases,
        cashIn: acc.cashIn + m.cashIn,
        cashOut: acc.cashOut + m.cashOut,
        bank: acc.bank + m.bank,
        credit: acc.credit + m.credit,
        totalIn: acc.totalIn + m.totalIn,
        totalOut: acc.totalOut + m.totalOut,
        txnCount: acc.txnCount + m.txnCount,
        netProfit: acc.netProfit + m.netProfit,
        netCash: acc.netCash + m.netCash,
      }),
      { sales: 0, expenses: 0, receipts: 0, purchases: 0, cashIn: 0, cashOut: 0, bank: 0, credit: 0, totalIn: 0, totalOut: 0, txnCount: 0, netProfit: 0, netCash: 0 }
    );

    return NextResponse.json({ summary, totals, monthCount: months.length });
  } catch (error) {
    console.error("GET /api/reports/monthly-summary error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
