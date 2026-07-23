import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { Prisma } from "@prisma/client";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";

    // Optimized: SQL aggregation instead of loading all transactions into memory
    const results = await prisma.$queryRaw<Array<{
      party_id: number;
      name: string;
      balance: number;
      last_receipt: Date | null;
      days_unpaid: number;
    }>>(Prisma.sql`
      SELECT
        p.party_id,
        p.name,
        COALESCE(SUM(CASE
          WHEN t.txn_type = 'Sale' THEN t.amount
          WHEN t.txn_type = 'Sale Return' THEN -t.amount
          WHEN t.txn_type = 'Receipt' THEN -t.amount
          ELSE 0
        END), 0) as balance,
        MAX(CASE WHEN t.txn_type = 'Receipt' THEN t.txn_date ELSE NULL END) as last_receipt,
        COALESCE(
          EXTRACT(DAY FROM NOW() - MAX(CASE WHEN t.txn_type = 'Receipt' THEN t.txn_date ELSE NULL END))::int,
          EXTRACT(DAY FROM NOW() - MAX(t.txn_date))::int,
          0
        ) as days_unpaid
      FROM parties p
      LEFT JOIN transactions t ON t.party_id = p.party_id AND t.company_id = p.company_id
      WHERE p.company_id = ${companyId}
        AND p.credit_allowed = true
        AND LOWER(TRIM(p.name)) != 'customer'
      GROUP BY p.party_id, p.name
      ORDER BY balance DESC
    `);

    const outstanding = results.map((r) => {
      const days = Number(r.days_unpaid) || 0;
      let status = "Normal";
      if (days >= 30) status = "Critical";
      else if (days >= 15) status = "High";

      return {
        partyId: r.party_id,
        name: r.name,
        balance: Number(r.balance),
        daysUnpaid: days,
        lastReceipt: r.last_receipt ? new Date(r.last_receipt).toISOString().split("T")[0] : null,
        status,
      };
    });

    const total = outstanding.reduce((s, p) => s + p.balance, 0);
    const highCount = outstanding.filter((p) => p.daysUnpaid >= 15).length;
    const criticalCount = outstanding.filter((p) => p.daysUnpaid >= 30).length;

    return NextResponse.json({ outstanding, total, highCount, criticalCount });
  } catch (error) {
    console.error("GET /api/reports/outstanding error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
