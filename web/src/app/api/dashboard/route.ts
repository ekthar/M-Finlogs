import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAuth } from "@/lib/auth-guard";
import { financialYearForDate } from "@/lib/financial-year";
import { Prisma } from "@prisma/client";

export async function GET(request: Request) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const financialYear = searchParams.get("financialYear") || financialYearForDate(new Date());

    const today = new Date();
    today.setHours(0, 0, 0, 0);
    const todayStr = today.toISOString().split("T")[0];
    const monthStart = new Date(today.getFullYear(), today.getMonth(), 1).toISOString().split("T")[0];

    // Single optimized raw query instead of 7 separate calls
    const result = await prisma.$queryRaw<Array<{
      today_sales: number;
      monthly_sales: number;
      cash_in: number;
      cash_out: number;
      bank_in: number;
      bank_out: number;
      upi_in: number;
      upi_out: number;
      credit_sales: number;
      total_receipts: number;
    }>>(Prisma.sql`
      SELECT
        COALESCE(SUM(CASE WHEN txn_date = ${today}::date AND txn_type = 'Sale' THEN amount ELSE 0 END), 0) as today_sales,
        COALESCE(SUM(CASE WHEN txn_date >= ${monthStart}::date AND txn_date <= ${todayStr}::date AND txn_type = 'Sale' THEN amount ELSE 0 END), 0) as monthly_sales,
        COALESCE(SUM(CASE WHEN payment_mode = 'Cash' AND txn_type IN ('Sale', 'Receipt') THEN amount ELSE 0 END), 0) as cash_in,
        COALESCE(SUM(CASE WHEN payment_mode = 'Cash' AND txn_type IN ('Expense', 'Sale Return') THEN amount ELSE 0 END), 0) as cash_out,
        COALESCE(SUM(CASE WHEN payment_mode = 'Bank' AND txn_type IN ('Sale', 'Receipt') THEN amount ELSE 0 END), 0) as bank_in,
        COALESCE(SUM(CASE WHEN payment_mode = 'Bank' AND txn_type IN ('Expense', 'Sale Return') THEN amount ELSE 0 END), 0) as bank_out,
        COALESCE(SUM(CASE WHEN payment_mode = 'UPI' AND txn_type IN ('Sale', 'Receipt') THEN amount ELSE 0 END), 0) as upi_in,
        COALESCE(SUM(CASE WHEN payment_mode = 'UPI' AND txn_type IN ('Expense', 'Sale Return') THEN amount ELSE 0 END), 0) as upi_out,
        COALESCE(SUM(CASE WHEN payment_mode = 'Credit' AND txn_type = 'Sale' THEN amount ELSE 0 END), 0) as credit_sales,
        COALESCE(SUM(CASE WHEN txn_type = 'Receipt' THEN amount ELSE 0 END), 0) as total_receipts
      FROM transactions
      WHERE company_id = ${companyId} AND financial_year = ${financialYear}
    `);

    const r = result[0] || { today_sales: 0, monthly_sales: 0, cash_in: 0, cash_out: 0, bank_in: 0, bank_out: 0, upi_in: 0, upi_out: 0, credit_sales: 0, total_receipts: 0 };

    const cashBalance = Number(r.cash_in) - Number(r.cash_out);
    const bankBalance = Number(r.bank_in) - Number(r.bank_out) + Number(r.upi_in) - Number(r.upi_out);
    const receivables = Math.max(0, Number(r.credit_sales) - Number(r.total_receipts));

    return NextResponse.json({
      todaySales: Number(r.today_sales),
      monthlySales: Number(r.monthly_sales),
      cashBalance,
      bankBalance,
      receivables,
      totalFunds: cashBalance + bankBalance,
    });
  } catch (error) {
    console.error("GET /api/dashboard error:", error);
    return NextResponse.json({ error: "Failed to fetch dashboard" }, { status: 500 });
  }
}
