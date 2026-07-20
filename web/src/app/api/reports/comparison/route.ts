import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { Prisma } from "@prisma/client";

/**
 * Report Comparison — compare two periods side by side.
 * Params: companyId, period1Start, period1End, period2Start, period2End
 */
export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const p1Start = searchParams.get("p1Start") || "";
    const p1End = searchParams.get("p1End") || "";
    const p2Start = searchParams.get("p2Start") || "";
    const p2End = searchParams.get("p2End") || "";

    if (!p1Start || !p1End || !p2Start || !p2End) {
      return NextResponse.json({ error: "All 4 date params required: p1Start, p1End, p2Start, p2End" }, { status: 400 });
    }

    const [period1, period2] = await Promise.all([
      prisma.$queryRaw<Array<{ sales: number; expenses: number; receipts: number; count: number }>>(Prisma.sql`
        SELECT
          COALESCE(SUM(CASE WHEN txn_type='Sale' THEN amount ELSE 0 END), 0) as sales,
          COALESCE(SUM(CASE WHEN txn_type='Expense' THEN amount ELSE 0 END), 0) as expenses,
          COALESCE(SUM(CASE WHEN txn_type='Receipt' THEN amount ELSE 0 END), 0) as receipts,
          COUNT(*) as count
        FROM transactions
        WHERE company_id=${companyId} AND txn_date >= ${p1Start}::date AND txn_date <= ${p1End}::date
      `),
      prisma.$queryRaw<Array<{ sales: number; expenses: number; receipts: number; count: number }>>(Prisma.sql`
        SELECT
          COALESCE(SUM(CASE WHEN txn_type='Sale' THEN amount ELSE 0 END), 0) as sales,
          COALESCE(SUM(CASE WHEN txn_type='Expense' THEN amount ELSE 0 END), 0) as expenses,
          COALESCE(SUM(CASE WHEN txn_type='Receipt' THEN amount ELSE 0 END), 0) as receipts,
          COUNT(*) as count
        FROM transactions
        WHERE company_id=${companyId} AND txn_date >= ${p2Start}::date AND txn_date <= ${p2End}::date
      `),
    ]);

    const p1 = period1[0] || { sales: 0, expenses: 0, receipts: 0, count: 0 };
    const p2 = period2[0] || { sales: 0, expenses: 0, receipts: 0, count: 0 };

    const comparison = {
      period1: { start: p1Start, end: p1End, sales: Number(p1.sales), expenses: Number(p1.expenses), receipts: Number(p1.receipts), transactions: Number(p1.count), profit: Number(p1.sales) - Number(p1.expenses) },
      period2: { start: p2Start, end: p2End, sales: Number(p2.sales), expenses: Number(p2.expenses), receipts: Number(p2.receipts), transactions: Number(p2.count), profit: Number(p2.sales) - Number(p2.expenses) },
      change: {
        sales: Number(p1.sales) > 0 ? Math.round(((Number(p2.sales) - Number(p1.sales)) / Number(p1.sales)) * 100) : 0,
        expenses: Number(p1.expenses) > 0 ? Math.round(((Number(p2.expenses) - Number(p1.expenses)) / Number(p1.expenses)) * 100) : 0,
        profit: Number(p1.sales) - Number(p1.expenses) > 0 ? Math.round((((Number(p2.sales) - Number(p2.expenses)) - (Number(p1.sales) - Number(p1.expenses))) / (Number(p1.sales) - Number(p1.expenses))) * 100) : 0,
      },
    };

    return NextResponse.json(comparison);
  } catch (error) {
    console.error("GET /api/reports/comparison error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
