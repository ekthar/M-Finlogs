import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAuth } from "@/lib/auth-guard";
import { financialYearForDate } from "@/lib/financial-year";
import { normalizePartyKey } from "@/lib/utils";

/**
 * Journal Entries (#48)
 * ─────────────────────
 * Double-entry journal voucher: creates TWO transactions (debit + credit)
 * in a single atomic operation.
 *
 * Use cases:
 * - Transfer between parties: Party A (Dr) ↔ Party B (Cr)
 * - Cash withdrawal to bank
 * - Adjustment entries
 * - Opening balance corrections
 *
 * POST body: {
 *   companyId, date, narration,
 *   debitParty, creditParty, amount,
 *   billNo? (optional)
 * }
 *
 * GET: List recent journal entries
 */
export async function GET(request: Request) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const limit = Math.min(parseInt(searchParams.get("limit") || "50"), 200);

    // Journal entries are identified by billNo starting with "JV-"
    const entries = await prisma.transaction.findMany({
      where: { companyId, billNo: { startsWith: "JV-" } },
      include: { party: { select: { name: true } } },
      orderBy: [{ txnDate: "desc" }, { txnId: "desc" }],
      take: limit,
    });

    // Group by JV number (pairs of debit + credit)
    const journalMap = new Map<string, typeof entries>();
    for (const e of entries) {
      const jvNo = e.billNo || "";
      if (!journalMap.has(jvNo)) journalMap.set(jvNo, []);
      journalMap.get(jvNo)!.push(e);
    }

    const journals = Array.from(journalMap.entries()).map(([jvNo, txns]) => ({
      jvNo,
      date: txns[0]?.txnDate,
      entries: txns.map(t => ({
        txnId: t.txnId,
        party: t.party.name,
        type: t.txnType,
        amount: Number(t.amount),
      })),
    }));

    return NextResponse.json({ journals, total: journals.length });
  } catch (error) {
    console.error("GET /api/journal error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}

export async function POST(request: Request) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { companyId = "cm_default_001", date, debitParty, creditParty, amount, narration, billNo } = await request.json();

    if (!date || !debitParty || !creditParty || !amount) {
      return NextResponse.json({ error: "date, debitParty, creditParty, and amount are required" }, { status: 400 });
    }

    const amt = parseFloat(amount);
    if (isNaN(amt) || amt <= 0) {
      return NextResponse.json({ error: "Amount must be positive" }, { status: 400 });
    }

    if (debitParty.trim().toLowerCase() === creditParty.trim().toLowerCase()) {
      return NextResponse.json({ error: "Debit and credit parties must be different" }, { status: 400 });
    }

    const txnDate = new Date(date);
    if (isNaN(txnDate.getTime())) {
      return NextResponse.json({ error: "Invalid date" }, { status: 400 });
    }

    const financialYear = financialYearForDate(txnDate);

    // Generate JV number
    const lastJV = await prisma.transaction.findFirst({
      where: { companyId, billNo: { startsWith: "JV-" } },
      orderBy: { txnId: "desc" },
      select: { billNo: true },
    });
    let jvNum = 1;
    if (lastJV?.billNo) {
      const match = lastJV.billNo.match(/JV-(\d+)/);
      if (match) jvNum = parseInt(match[1]) + 1;
    }
    const jvNo = billNo || `JV-${String(jvNum).padStart(4, "0")}`;

    // ── Find or create debit party ──
    const debitNormalized = normalizePartyKey(debitParty);
    let debitRecord = await prisma.party.findFirst({ where: { normalizedName: debitNormalized, companyId } });
    if (!debitRecord) {
      debitRecord = await prisma.party.create({
        data: { name: debitParty.trim(), normalizedName: debitNormalized, type: "Customer", creditAllowed: true, companyId },
      });
    }

    // ── Find or create credit party ──
    const creditNormalized = normalizePartyKey(creditParty);
    let creditRecord = await prisma.party.findFirst({ where: { normalizedName: creditNormalized, companyId } });
    if (!creditRecord) {
      creditRecord = await prisma.party.create({
        data: { name: creditParty.trim(), normalizedName: creditNormalized, type: "Customer", creditAllowed: true, companyId },
      });
    }

    // ── Create paired transactions atomically ──
    const [debitTxn, creditTxn] = await prisma.$transaction([
      // Debit entry (increases debit party's balance)
      prisma.transaction.create({
        data: {
          txnDate,
          billNo: jvNo,
          partyId: debitRecord.partyId,
          txnType: "Sale", // Debit side shown as Sale (increases balance)
          paymentMode: "Credit",
          financialYear,
          amount: amt,
          companyId,
        },
      }),
      // Credit entry (decreases credit party's balance)
      prisma.transaction.create({
        data: {
          txnDate,
          billNo: jvNo,
          partyId: creditRecord.partyId,
          txnType: "Receipt", // Credit side shown as Receipt (decreases balance)
          paymentMode: "Credit",
          financialYear,
          amount: amt,
          companyId,
        },
      }),
    ]);

    // ── Audit log ──
    try {
      const username = (auth as any).username || "system";
      await prisma.auditLog.create({
        data: {
          username,
          action: "JOURNAL_ENTRY",
          details: `${jvNo}: ${debitParty} (Dr) ₹${amt} / ${creditParty} (Cr) ₹${amt}${narration ? ` — ${narration}` : ""}`,
          companyId,
        },
      });
    } catch {}

    return NextResponse.json({
      status: "Journal entry created",
      jvNo,
      debitTxnId: debitTxn.txnId,
      creditTxnId: creditTxn.txnId,
      amount: amt,
      debitParty: debitParty.trim(),
      creditParty: creditParty.trim(),
    }, { status: 201 });
  } catch (error) {
    console.error("POST /api/journal error:", error);
    return NextResponse.json({ error: "Failed to create journal entry" }, { status: 500 });
  }
}
