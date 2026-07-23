import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAuth } from "@/lib/auth-guard";

/**
 * Profit & Loss — Fixed accounting logic:
 * - Revenue = Sales - Sale Returns
 * - COGS = Purchases
 * - Gross Profit = Revenue - COGS
 * - Net Profit = Gross Profit - Expenses
 * - Filters by financial year and company
 */
export async function GET(request: Request) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const financialYear = searchParams.get("financialYear") || "";

    const where: Record<string, unknown> = { companyId };
    if (financialYear) where.financialYear = financialYear;

    const [sales, saleReturns, expenses, purchases, receipts] = await Promise.all([
      prisma.transaction.aggregate({ where: { ...where, txnType: "Sale" }, _sum: { amount: true } }),
      prisma.transaction.aggregate({ where: { ...where, txnType: "Sale Return" }, _sum: { amount: true } }),
      prisma.transaction.aggregate({ where: { ...where, txnType: "Expense" }, _sum: { amount: true } }),
      prisma.transaction.aggregate({ where: { ...where, txnType: "Purchase" }, _sum: { amount: true } }),
      prisma.transaction.aggregate({ where: { ...where, txnType: "Receipt" }, _sum: { amount: true } }),
    ]);

    const totalSales = Number(sales._sum.amount || 0);
    const totalReturns = Number(saleReturns._sum.amount || 0);
    const totalExpenses = Number(expenses._sum.amount || 0);
    const totalPurchases = Number(purchases._sum.amount || 0);
    const totalReceipts = Number(receipts._sum.amount || 0);

    const revenue = totalSales - totalReturns;
    const grossProfit = revenue - totalPurchases;
    const netProfit = grossProfit - totalExpenses;

    return NextResponse.json({
      totalSales,
      totalReturns,
      revenue,
      totalPurchases,
      grossProfit,
      totalExpenses,
      netProfit,
      totalReceipts,
    });
  } catch (error) {
    console.error("GET /api/reports/profit-loss error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
