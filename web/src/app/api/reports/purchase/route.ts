import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAuth } from "@/lib/auth-guard";
import { Prisma } from "@prisma/client";

export async function GET(request: Request) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const financialYear = searchParams.get("financialYear") || "";
    const startDate = searchParams.get("start") || "";
    const endDate = searchParams.get("end") || "";

    const fyFilter = financialYear ? Prisma.sql`AND t.financial_year = ${financialYear}` : Prisma.sql``;
    const dateFilter = (startDate && endDate)
      ? Prisma.sql`AND t.txn_date >= ${startDate}::date AND t.txn_date <= ${endDate}::date`
      : Prisma.sql``;

    // Monthly purchase totals
    const monthly = await prisma.$queryRaw<Array<{ month: string; total: number }>>(Prisma.sql`
      SELECT TO_CHAR(t.txn_date, 'YYYY-MM') as month, SUM(t.amount) as total
      FROM transactions t
      WHERE t.company_id = ${companyId} AND t.txn_type = 'Expense' ${fyFilter} ${dateFilter}
      GROUP BY TO_CHAR(t.txn_date, 'YYYY-MM')
      ORDER BY month DESC
    `);

    // Party-wise purchase
    const partyWise = await prisma.$queryRaw<Array<{ name: string; total: number }>>(Prisma.sql`
      SELECT p.name, SUM(t.amount) as total
      FROM transactions t
      INNER JOIN parties p ON p.party_id = t.party_id
      WHERE t.company_id = ${companyId} AND t.txn_type = 'Expense' ${fyFilter} ${dateFilter}
      GROUP BY p.name
      ORDER BY total DESC
    `);

    const grandTotal = monthly.reduce((s, m) => s + Number(m.total), 0);

    return NextResponse.json({
      monthly: monthly.map(m => ({ month: m.month, total: Number(m.total) })),
      partyWise: partyWise.map(p => ({ name: p.name, total: Number(p.total) })),
      grandTotal,
    });
  } catch (error) {
    console.error("GET /api/reports/purchase error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
