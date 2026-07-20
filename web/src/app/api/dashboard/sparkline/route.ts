import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { Prisma } from "@prisma/client";

/**
 * Dashboard Sparklines — returns last 7 days of daily totals
 * for sales, expenses, cash, receipts (used as mini-charts in metric cards).
 */
export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";

    const result = await prisma.$queryRaw<Array<{
      date: string;
      sales: number;
      expenses: number;
      cash: number;
      receipts: number;
    }>>(Prisma.sql`
      SELECT
        txn_date::text as date,
        COALESCE(SUM(CASE WHEN txn_type = 'Sale' THEN amount ELSE 0 END), 0) as sales,
        COALESCE(SUM(CASE WHEN txn_type = 'Expense' THEN amount ELSE 0 END), 0) as expenses,
        COALESCE(SUM(CASE WHEN payment_mode = 'Cash' AND txn_type IN ('Sale','Receipt') THEN amount ELSE 0 END), 0) as cash,
        COALESCE(SUM(CASE WHEN txn_type = 'Receipt' THEN amount ELSE 0 END), 0) as receipts
      FROM transactions
      WHERE company_id = ${companyId}
        AND txn_date >= CURRENT_DATE - 7
        AND txn_date <= CURRENT_DATE
      GROUP BY txn_date
      ORDER BY txn_date ASC
    `);

    return NextResponse.json({
      sales: result.map(r => Number(r.sales)),
      expenses: result.map(r => Number(r.expenses)),
      cash: result.map(r => Number(r.cash)),
      receipts: result.map(r => Number(r.receipts)),
    });
  } catch (error) {
    console.error("GET /api/dashboard/sparkline error:", error);
    return NextResponse.json({ sales: [], expenses: [], cash: [], receipts: [] });
  }
}
