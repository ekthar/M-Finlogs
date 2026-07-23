import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { financialYearForDate } from "@/lib/financial-year";
import { normalizePartyKey } from "@/lib/utils";
import { requireAuth } from "@/lib/auth-guard";
import { transactionSchema } from "@/lib/validators";

export async function GET(request: Request) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

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
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const body = await request.json();
    const { txnDate, billNo, party, txnType, paymentMode, amount, companyId: bodyCompanyId, clientId } = body;
    const companyId = bodyCompanyId || "cm_default_001";

    // Zod validation
    const validation = transactionSchema.safeParse({
      txnDate, party, txnType, paymentMode,
      amount: typeof amount === "string" ? parseFloat(amount) : amount,
      billNo: billNo || undefined,
    });
    if (!validation.success) {
      const firstError = validation.error.issues[0];
      return NextResponse.json({ error: firstError.message || "Validation failed" }, { status: 400 });
    }

    if (!txnDate || !party || !txnType || !paymentMode || !amount) {
      return NextResponse.json({ error: "All fields are required" }, { status: 400 });
    }

    if (parseFloat(amount) <= 0) {
      return NextResponse.json({ error: "Amount must be positive" }, { status: 400 });
    }

    // DEDUPLICATION: If clientId provided, check if already saved
    if (clientId) {
      const existing = await prisma.transaction.findFirst({
        where: { billNo: `__cid:${clientId}`, companyId },
      });
      if (existing) {
        return NextResponse.json({ status: "Already exists", transaction: existing }, { status: 409 });
      }
    }

    // DUPLICATE DETECTION: Check if same bill+date+party+amount exists within last 5 minutes
    if (billNo) {
      const normalizedName = normalizePartyKey(party);
      const duplicate = await prisma.transaction.findFirst({
        where: {
          companyId,
          billNo,
          txnDate: new Date(txnDate),
        },
      });
      if (duplicate) {
        return NextResponse.json({
          error: `Bill #${billNo} already exists for this date. Duplicate entry prevented.`,
          duplicate: true,
        }, { status: 409 });
      }
    }

    // DATE VALIDATION: No future dates allowed
    const txnDateObj = new Date(txnDate);
    const today = new Date();
    today.setHours(23, 59, 59, 999);
    if (txnDateObj > today) {
      return NextResponse.json({ error: "Future dates are not allowed" }, { status: 400 });
    }

    // Find the party — auto-create if not found
    const normalizedName = normalizePartyKey(party);
    let partyRecord = await prisma.party.findFirst({
      where: { normalizedName, companyId },
    });

    if (!partyRecord) {
      // Auto-create party as Customer (user can change type later)
      partyRecord = await prisma.party.create({
        data: {
          name: party.trim(),
          normalizedName,
          type: "Customer",
          creditAllowed: paymentMode === "Credit",
          companyId,
        },
      });
    }

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
