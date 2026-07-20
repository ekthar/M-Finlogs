import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "";

    const where: Record<string, unknown> = { creditAllowed: true };
    if (companyId) where.companyId = companyId;

    const parties = await prisma.party.findMany({
      where,
      include: {
        transactions: {
          orderBy: { txnDate: "desc" },
        },
      },
    });

    const today = new Date();
    const results = parties.map((party) => {
      let balance = 0;
      let lastReceiptDate: Date | null = null;

      for (const t of party.transactions) {
        const amt = Number(t.amount);
        if (t.txnType === "Sale") balance += amt;
        else if (t.txnType === "Receipt") {
          balance -= amt;
          if (!lastReceiptDate || new Date(t.txnDate) > lastReceiptDate) {
            lastReceiptDate = new Date(t.txnDate);
          }
        } else if (t.txnType === "Sale Return") balance -= amt;
      }

      const daysSinceReceipt = lastReceiptDate
        ? Math.floor((today.getTime() - lastReceiptDate.getTime()) / 86400000)
        : party.transactions.length > 0
          ? Math.floor((today.getTime() - new Date(party.transactions[0].txnDate).getTime()) / 86400000)
          : 0;

      let status = "Normal";
      if (daysSinceReceipt >= 30) status = "Critical";
      else if (daysSinceReceipt >= 15) status = "High";

      return {
        partyId: party.partyId,
        name: party.name,
        balance: Math.max(0, balance),
        daysUnpaid: daysSinceReceipt,
        lastReceipt: lastReceiptDate ? lastReceiptDate.toISOString().split("T")[0] : null,
        status,
      };
    }).filter((p) => p.balance > 0);

    const total = results.reduce((sum, p) => sum + p.balance, 0);
    const highCount = results.filter((p) => p.daysUnpaid >= 15).length;
    const criticalCount = results.filter((p) => p.daysUnpaid >= 30).length;

    return NextResponse.json({ outstanding: results, total, highCount, criticalCount });
  } catch (error) {
    console.error("GET /api/reports/outstanding error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
