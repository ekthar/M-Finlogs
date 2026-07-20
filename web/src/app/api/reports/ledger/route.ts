import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const partyName = searchParams.get("party") || "";
    const startDate = searchParams.get("start") || "";
    const endDate = searchParams.get("end") || "";
    const companyId = searchParams.get("companyId") || "";

    if (!partyName) {
      return NextResponse.json({ error: "Party name is required" }, { status: 400 });
    }

    const normalized = partyName.trim().toLowerCase().replace(/\s+/g, "_");
    const party = await prisma.party.findFirst({
      where: { normalizedName: normalized, ...(companyId ? { companyId } : {}) },
    });

    if (!party) {
      return NextResponse.json({ error: "Party not found" }, { status: 404 });
    }

    const where: Record<string, unknown> = { partyId: party.partyId };
    if (startDate && endDate) {
      where.txnDate = { gte: new Date(startDate), lte: new Date(endDate) };
    }

    const transactions = await prisma.transaction.findMany({
      where,
      orderBy: [{ txnDate: "asc" }, { txnId: "asc" }],
    });

    // Build ledger with running balance
    let balance = 0;
    const ledger = transactions.map((t) => {
      const amt = Number(t.amount);
      let debit = 0;
      let credit = 0;

      if (t.txnType === "Sale" || t.txnType === "Expense") {
        debit = amt;
        balance += amt;
      } else {
        credit = amt;
        balance -= amt;
      }

      return {
        date: t.txnDate,
        billNo: t.billNo,
        txnType: t.txnType,
        paymentMode: t.paymentMode,
        debit,
        credit,
        balance,
      };
    });

    return NextResponse.json({ party: party.name, ledger, totalBalance: balance });
  } catch (error) {
    console.error("GET /api/reports/ledger error:", error);
    return NextResponse.json({ error: "Failed to fetch ledger" }, { status: 500 });
  }
}
