import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAuth } from "@/lib/auth-guard";

export async function GET(request: Request) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const financialYear = searchParams.get("financialYear") || "";
    const startDate = searchParams.get("start") || "";
    const endDate = searchParams.get("end") || "";

    const where: Record<string, unknown> = { companyId, txnType: "Expense" };
    if (financialYear) where.financialYear = financialYear;
    if (startDate && endDate) where.txnDate = { gte: new Date(startDate), lte: new Date(endDate) };

    const expenses = await prisma.transaction.findMany({
      where,
      include: { party: { select: { name: true } } },
      orderBy: [{ txnDate: "desc" }, { txnId: "desc" }],
      take: 500,
    });

    const total = expenses.reduce((s, e) => s + Number(e.amount), 0);

    return NextResponse.json({
      expenses: expenses.map(e => ({
        txnId: e.txnId,
        date: e.txnDate,
        party: e.party.name,
        paymentMode: e.paymentMode,
        billNo: e.billNo,
        amount: Number(e.amount),
      })),
      total,
    });
  } catch (error) {
    console.error("GET /api/reports/expenses error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
