import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { Prisma } from "@prisma/client";

/**
 * Notifications API — generates alerts based on business rules.
 * No separate table needed — computes from existing data.
 */
export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";

    const notifications: { id: string; type: string; title: string; description: string; severity: "info" | "warning" | "danger"; time: string }[] = [];

    // 1. Overdue parties (30+ days)
    const overdue = await prisma.$queryRaw<Array<{ name: string; days: number; balance: number }>>(Prisma.sql`
      SELECT p.name,
        COALESCE(EXTRACT(DAY FROM NOW() - MAX(CASE WHEN t.txn_type='Receipt' THEN t.txn_date END))::int, 999) as days,
        COALESCE(SUM(CASE WHEN t.txn_type='Sale' THEN t.amount WHEN t.txn_type IN ('Receipt','Sale Return') THEN -t.amount ELSE 0 END), 0) as balance
      FROM parties p LEFT JOIN transactions t ON t.party_id=p.party_id AND t.company_id=p.company_id
      WHERE p.company_id=${companyId} AND p.credit_allowed=true
      GROUP BY p.party_id, p.name
      HAVING COALESCE(SUM(CASE WHEN t.txn_type='Sale' THEN t.amount WHEN t.txn_type IN ('Receipt','Sale Return') THEN -t.amount ELSE 0 END), 0) > 0
        AND COALESCE(EXTRACT(DAY FROM NOW() - MAX(CASE WHEN t.txn_type='Receipt' THEN t.txn_date END))::int, 999) >= 30
      ORDER BY balance DESC
      LIMIT 5
    `);

    for (const o of overdue) {
      notifications.push({
        id: `overdue-${o.name}`,
        type: "overdue",
        title: `${o.name} — ${Number(o.days)} days overdue`,
        description: `Outstanding: ₹${Number(o.balance).toLocaleString("en-IN")}`,
        severity: Number(o.days) >= 60 ? "danger" : "warning",
        time: "now",
      });
    }

    // 2. Today's summary
    const todayResult = await prisma.$queryRaw<Array<{ sales: number; receipts: number; expenses: number }>>(Prisma.sql`
      SELECT
        COALESCE(SUM(CASE WHEN txn_type='Sale' THEN amount ELSE 0 END), 0) as sales,
        COALESCE(SUM(CASE WHEN txn_type='Receipt' THEN amount ELSE 0 END), 0) as receipts,
        COALESCE(SUM(CASE WHEN txn_type='Expense' THEN amount ELSE 0 END), 0) as expenses
      FROM transactions WHERE company_id=${companyId} AND txn_date=CURRENT_DATE
    `);

    if (todayResult[0]) {
      const t = todayResult[0];
      notifications.push({
        id: "today-summary",
        type: "summary",
        title: "Today's Activity",
        description: `Sales: ₹${Number(t.sales).toLocaleString("en-IN")} | Receipts: ₹${Number(t.receipts).toLocaleString("en-IN")} | Expenses: ₹${Number(t.expenses).toLocaleString("en-IN")}`,
        severity: "info",
        time: "today",
      });
    }

    return NextResponse.json({ notifications, count: notifications.length });
  } catch (error) {
    console.error("GET /api/notifications error:", error);
    return NextResponse.json({ notifications: [], count: 0 });
  }
}
