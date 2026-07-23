import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAuth } from "@/lib/auth-guard";

export async function GET(request: Request) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { searchParams } = new URL(request.url);
    const startDate = searchParams.get("start") || "";
    const endDate = searchParams.get("end") || "";
    const companyId = searchParams.get("companyId") || "cm_default_001";

    const end = endDate ? new Date(endDate) : new Date();
    const start = startDate ? new Date(startDate) : new Date(end.getTime() - 29 * 86400000);

    const transactions = await prisma.transaction.findMany({
      where: { txnDate: { gte: start, lte: end }, companyId },
      orderBy: { txnDate: "asc" },
    });

    const cashRecords = await prisma.dailyCash.findMany({
      where: { cashDate: { gte: start, lte: end }, companyId },
    });
    const cashMap: Record<string, number> = {};
    for (const c of cashRecords) {
      cashMap[new Date(c.cashDate).toISOString().split("T")[0]] = Number(c.cashInHand);
    }

    const byDate: Record<string, { cashIn: number; cashExp: number }> = {};
    for (const t of transactions) {
      const key = new Date(t.txnDate).toISOString().split("T")[0];
      if (!byDate[key]) byDate[key] = { cashIn: 0, cashExp: 0 };
      const amt = Number(t.amount);
      if ((t.txnType === "Sale" || t.txnType === "Receipt") && t.paymentMode === "Cash") byDate[key].cashIn += amt;
      if ((t.txnType === "Expense" || t.txnType === "Sale Return") && t.paymentMode === "Cash") byDate[key].cashExp += amt;
    }

    let runningCash = 0;
    const rows = Object.keys(byDate).sort().map((date) => {
      const d = byDate[date];
      const opening = runningCash;
      const cashNeeded = opening + d.cashIn - d.cashExp;
      const cashInHand = cashMap[date] !== undefined ? cashMap[date] : cashNeeded;
      const shortExcess = cashInHand - cashNeeded;
      runningCash = cashInHand;
      return { date, openingCash: opening, cashIn: d.cashIn, cashExp: d.cashExp, cashNeeded, cashInHand, shortExcess };
    });

    const totalShort = rows.filter(r => r.shortExcess < 0).reduce((s, r) => s + r.shortExcess, 0);
    const totalExcess = rows.filter(r => r.shortExcess > 0).reduce((s, r) => s + r.shortExcess, 0);

    return NextResponse.json({ rows, totalShort, totalExcess });
  } catch (error) {
    console.error("GET /api/reports/short-report error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
