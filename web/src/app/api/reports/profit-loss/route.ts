import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "";
    const financialYear = searchParams.get("financialYear") || "";

    const where: Record<string, unknown> = {};
    if (companyId) where.companyId = companyId;
    if (financialYear) where.financialYear = financialYear;

    const sales = await prisma.transaction.aggregate({
      where: { ...where, txnType: "Sale" },
      _sum: { amount: true },
    });

    const saleReturns = await prisma.transaction.aggregate({
      where: { ...where, txnType: "Sale Return" },
      _sum: { amount: true },
    });

    const expenses = await prisma.transaction.aggregate({
      where: { ...where, txnType: "Expense" },
      _sum: { amount: true },
    });

    const totalSales = Number(sales._sum.amount || 0) - Number(saleReturns._sum.amount || 0);
    const totalExpenses = Number(expenses._sum.amount || 0);
    const netProfit = totalSales - totalExpenses;

    return NextResponse.json({ totalSales, totalExpenses, netProfit });
  } catch (error) {
    console.error("GET /api/reports/profit-loss error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
