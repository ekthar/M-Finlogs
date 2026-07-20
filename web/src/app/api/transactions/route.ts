import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { transactionSchema } from "@/lib/validators";
import { financialYearForDate } from "@/lib/financial-year";
import { normalizePartyKey } from "@/lib/utils";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const page = parseInt(searchParams.get("page") || "1");
    const limit = parseInt(searchParams.get("limit") || "50");
    const companyId = searchParams.get("companyId") || "";
    const financialYear = searchParams.get("financialYear") || "";

    const where: Record<string, unknown> = {};
    if (companyId) where.companyId = companyId;
    if (financialYear) where.financialYear = financialYear;

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
    const parsed = transactionSchema.safeParse(body);

    if (!parsed.success) {
      return NextResponse.json(
        { error: "Invalid input", details: parsed.error.flatten() },
        { status: 400 }
      );
    }

    const { txnDate, billNo, party, txnType, paymentMode, amount } = parsed.data;
    const companyId = body.companyId || "";

    // Find the party
    const normalizedName = normalizePartyKey(party);
    const partyRecord = await prisma.party.findFirst({
      where: { normalizedName, companyId },
    });

    if (!partyRecord) {
      return NextResponse.json({ error: "Party not found" }, { status: 404 });
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
        amount,
        companyId,
      },
    });

    return NextResponse.json({ status: "Transaction Added", transaction }, { status: 201 });
  } catch (error) {
    console.error("POST /api/transactions error:", error);
    return NextResponse.json({ error: "Failed to create transaction" }, { status: 500 });
  }
}
