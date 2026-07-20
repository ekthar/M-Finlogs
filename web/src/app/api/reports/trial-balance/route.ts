import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { Prisma } from "@prisma/client";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const financialYear = searchParams.get("financialYear") || "";

    // Optimized: SQL groupBy aggregation instead of loading all transactions
    const fyFilter = financialYear ? Prisma.sql`AND t.financial_year = ${financialYear}` : Prisma.sql``;

    const results = await prisma.$queryRaw<Array<{
      name: string;
      type: string;
      debit: number;
      credit: number;
    }>>(Prisma.sql`
      SELECT
        p.name,
        p.type,
        COALESCE(SUM(CASE WHEN t.txn_type IN ('Sale', 'Expense') THEN t.amount ELSE 0 END), 0) as debit,
        COALESCE(SUM(CASE WHEN t.txn_type IN ('Receipt', 'Sale Return') THEN t.amount ELSE 0 END), 0) as credit
      FROM parties p
      INNER JOIN transactions t ON t.party_id = p.party_id AND t.company_id = p.company_id
      WHERE p.company_id = ${companyId} ${fyFilter}
      GROUP BY p.party_id, p.name, p.type
      HAVING SUM(t.amount) > 0
      ORDER BY p.name
    `);

    const accounts = results.map((r) => ({
      name: r.name,
      type: r.type,
      debit: Number(r.debit),
      credit: Number(r.credit),
    }));

    const totalDebit = accounts.reduce((s, a) => s + a.debit, 0);
    const totalCredit = accounts.reduce((s, a) => s + a.credit, 0);

    return NextResponse.json({ accounts, totalDebit, totalCredit });
  } catch (error) {
    console.error("GET /api/reports/trial-balance error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
