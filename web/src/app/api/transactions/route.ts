import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { financialYearForDate } from "@/lib/financial-year";
import { normalizePartyKey } from "@/lib/utils";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const page = parseInt(searchParams.get("page") || "1");
    const limit = Math.min(parseInt(searchParams.get("limit") || "50"), 100);
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const financialYear = searchParams.get("financialYear") || "";
    const startDate = searchParams.get("start") || "";
    const endDate = searchParams.get("end") || "";

    const where: Record<string, unknown> = { companyId };
    if (financialYear) where.financialYear = financialYear;
    if (startDate && endDate) {
      where.txnDate = { gte: new Date(startDate), lte: new Date(endDate) };
    } else if (startDate) {
      where.txnDate = { gte: new Date(startDate) };
    } else if (endDate) {
      where.txnDate = { lte: new Date(endDate) };
    }

    const [transactions, total] = await Promise.all([
      prisma.transaction.findMany({
        where,
        include: { party: { select: { name: true, type: true } } },
        orderBy: [{ txnDate: "desc" }, { txnId: "desc" }],
        skip: (page - 1) * limit,
        take: limit,
      }),
      prisma.transaction.count({ where }),
    ]);

    return NextResponse.json({
      transactions,
      total,
      page,
      totalPages: Math.ceil(total / limit),
    });
  } catch (error) {
    console.error("GET /api/transactions error:", error);
    return NextResponse.json({ error: "Failed to fetch transactions" }, { status: 500 });
  }
}

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const { txnDate, billNo, party, txnType, paymentMode, amount, companyId: bodyCompanyId } = body;
    const companyId = bodyCompanyId || "cm_default_001";

    if (!txnDate || !party || !txnType || !paymentMode || !amount) {
      return NextResponse.json({ error: "All fields are required" }, { status: 400 });
    }

    if (parseFloat(amount) <= 0) {
      return NextResponse.json({ error: "Amount must be positive" }, { status: 400 });
    }

    // Find the party
    const normalizedName = normalizePartyKey(party);
    const partyRecord = await prisma.party.findFirst({
      where: { normalizedName, companyId },
    });

    if (!partyRecord) {
      return NextResponse.json({ error: `Party "${party}" not found. Create it first.` }, { status: 404 });
    }

    const txnDateObj = new Date(txnDate);
    const financialYear = financialYearForDate(txnDateObj);

    const transaction = await prisma.transaction.create({
      data: {
        txnDate: txnDateObj,
        billNo: billNo || null,
        partyId: partyRecord.partyId,
        txnType,
        paymentMode,
        financialYear,
        amount: parseFloat(amount),
        companyId,
      },
    });

    return NextResponse.json({ status: "Transaction Added", transaction }, { status: 201 });
  } catch (error) {
    console.error("POST /api/transactions error:", error);
    return NextResponse.json({ error: "Failed to create transaction" }, { status: 500 });
  }
}
