import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "";
    const financialYear = searchParams.get("financialYear") || "";

    const where: Record<string, unknown> = {};
    if (companyId) where.companyId = companyId;
    if (financialYear) where.financialYear = financialYear;

    const parties = await prisma.party.findMany({
      where: companyId ? { companyId } : {},
      include: {
        transactions: { where: financialYear ? { financialYear } : {} },
      },
    });

    const accounts = parties.map((party) => {
      let debit = 0;
      let credit = 0;

      for (const t of party.transactions) {
        const amt = Number(t.amount);
        if (t.txnType === "Sale" || t.txnType === "Expense") {
          debit += amt;
        } else {
          credit += amt;
        }
      }

      return { name: party.name, type: party.type, debit, credit };
    }).filter((a) => a.debit > 0 || a.credit > 0);

    const totalDebit = accounts.reduce((s, a) => s + a.debit, 0);
    const totalCredit = accounts.reduce((s, a) => s + a.credit, 0);

    return NextResponse.json({ accounts, totalDebit, totalCredit });
  } catch (error) {
    console.error("GET /api/reports/trial-balance error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
