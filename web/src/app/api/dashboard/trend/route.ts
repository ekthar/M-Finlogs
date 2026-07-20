import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { Prisma } from "@prisma/client";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const days = parseInt(searchParams.get("days") || "30");

    const result = await prisma.$queryRaw<Array<{
      date: string;
      sales: number;
      expenses: number;
      receipts: number;
    }>>(Prisma.sql`
      SELECT
        txn_date::text as date,
        COALESCE(SUM(CASE WHEN txn_type = 'Sale' THEN amount ELSE 0 END), 0) as sales,
        COALESCE(SUM(CASE WHEN txn_type = 'Expense' THEN amount ELSE 0 END), 0) as expenses,
        COALESCE(SUM(CASE WHEN txn_type = 'Receipt' THEN amount ELSE 0 END), 0) as receipts
      FROM transactions
      WHERE company_id = ${companyId}
        AND txn_date >= CURRENT_DATE - ${days}::int
        AND txn_date <= CURRENT_DATE
      GROUP BY txn_date
      ORDER BY txn_date ASC
    `);

    return NextResponse.json({
      trend: result.map(r => ({
        date: r.date,
        sales: Number(r.sales),
        expenses: Number(r.expenses),
        receipts: Number(r.receipts),
      })),
    });
  } catch (error) {
    console.error("GET /api/dashboard/trend error:", error);
    return NextResponse.json({ trend: [] }, { status: 200 });
  }
}
