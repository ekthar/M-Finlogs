import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { financialYearForDate } from "@/lib/financial-year";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "";

    const today = new Date();
    today.setHours(0, 0, 0, 0);
    const monthStart = new Date(today.getFullYear(), today.getMonth(), 1);
    const financialYear = financialYearForDate(today);

    const where = { companyId, financialYear };

    // Today's sales
    const todaySales = await prisma.transaction.aggregate({
      where: { ...where, txnDate: today, txnType: "Sale" },
      _sum: { amount: true },
    });

    // Monthly sales
    const monthlySales = await prisma.transaction.aggregate({
      where: {
        ...where,
        txnType: "Sale",
        txnDate: { gte: monthStart, lte: today },
      },
      _sum: { amount: true },
    });

    // Cash balance (sum of cash receipts - cash expenses)
    const cashIn = await prisma.transaction.aggregate({
      where: { ...where, paymentMode: "Cash", txnType: { in: ["Sale", "Receipt"] } },
      _sum: { amount: true },
    });
    const cashOut = await prisma.transaction.aggregate({
      where: { ...where, paymentMode: "Cash", txnType: { in: ["Expense", "Sale Return"] } },
      _sum: { amount: true },
    });

    // Bank balance
    const bankIn = await prisma.transaction.aggregate({
      where: { ...where, paymentMode: "Bank", txnType: { in: ["Sale", "Receipt"] } },
      _sum: { amount: true },
    });
    const bankOut = await prisma.transaction.aggregate({
      where: { ...where, paymentMode: "Bank", txnType: { in: ["Expense", "Sale Return"] } },
      _sum: { amount: true },
    });

    // Receivables (credit sales - receipts)
    const creditSales = await prisma.transaction.aggregate({
      where: { ...where, paymentMode: "Credit", txnType: "Sale" },
      _sum: { amount: true },
    });
    const receipts = await prisma.transaction.aggregate({
      where: { ...where, txnType: "Receipt" },
      _sum: { amount: true },
    });

    const cashBalance =
      Number(cashIn._sum.amount || 0) - Number(cashOut._sum.amount || 0);
    const bankBalance =
      Number(bankIn._sum.amount || 0) - Number(bankOut._sum.amount || 0);
    const receivables =
      Number(creditSales._sum.amount || 0) - Number(receipts._sum.amount || 0);

    return NextResponse.json({
      todaySales: Number(todaySales._sum.amount || 0),
      monthlySales: Number(monthlySales._sum.amount || 0),
      cashBalance,
      bankBalance,
      receivables: Math.max(0, receivables),
      totalFunds: cashBalance + bankBalance,
    });
  } catch (error) {
    console.error("GET /api/dashboard error:", error);
    return NextResponse.json({ error: "Failed to fetch dashboard" }, { status: 500 });
  }
}
