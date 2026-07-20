import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const date = searchParams.get("date") || new Date().toISOString().split("T")[0];
    const companyId = searchParams.get("companyId") || "";

    const where: Record<string, unknown> = { txnDate: new Date(date) };
    if (companyId) where.companyId = companyId;

    const transactions = await prisma.transaction.findMany({
      where,
      include: { party: { select: { name: true } } },
      orderBy: { txnId: "asc" },
    });

    const total = transactions.reduce((sum, t) => sum + Number(t.amount), 0);

    return NextResponse.json({
      date,
      transactions: transactions.map((t) => ({
        txnId: t.txnId,
        date: t.txnDate,
        billNo: t.billNo,
        party: t.party.name,
        txnType: t.txnType,
        paymentMode: t.paymentMode,
        amount: Number(t.amount),
      })),
      total,
    });
  } catch (error) {
    console.error("GET /api/reports/day-book error:", error);
    return NextResponse.json({ error: "Failed to fetch day book" }, { status: 500 });
  }
}
