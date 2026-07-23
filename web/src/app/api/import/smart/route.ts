import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAdmin } from "@/lib/auth-guard";
import { financialYearForDate } from "@/lib/financial-year";
import { normalizePartyKey } from "@/lib/utils";
import { cleanPartyName, matchPartyName } from "@/lib/import-utils";

/**
 * Smart Import — Daily Register Format (COL / SL:L)
 * ──────────────────────────────────────────────────
 * Accepts pre-parsed smart import rows from the client.
 * The client detects the format and splits transactions by mode.
 *
 * Body: {
 *   companyId,
 *   transactions: [{ billNo, type, date, party, paymentMode, amount }]
 * }
 *
 * Features:
 * - Party name cleaning + typo correction
 * - Auto-creates parties (creditAllowed=true for credit sales)
 * - Duplicate detection by billNo + date + type
 * - Financial year auto-calculation
 */
export async function POST(request: Request) {
  const auth = await requireAdmin(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { companyId = "cm_default_001", transactions } = await request.json();

    if (!Array.isArray(transactions) || transactions.length === 0) {
      return NextResponse.json({ error: "transactions array required" }, { status: 400 });
    }

    // Pre-fetch existing parties
    const existingParties = await prisma.party.findMany({
      where: { companyId },
      select: { partyId: true, name: true, normalizedName: true },
    });
    const partyNames = existingParties.map((p) => p.name);
    const partyCache = new Map<string, number>();
    for (const p of existingParties) {
      partyCache.set(p.normalizedName, p.partyId);
    }

    let receipts = 0;
    let cashSales = 0;
    let creditSales = 0;
    let upiSales = 0;
    let updated = 0;
    let partiesCreated = 0;
    let skipped = 0;
    const errors: { row: number; reason: string }[] = [];

    let totalReceipts = 0;
    let totalCashSales = 0;
    let totalCreditSales = 0;
    let totalUpiSales = 0;

    for (let i = 0; i < transactions.length; i++) {
      const txn = transactions[i];
      const rowNum = i + 1;

      try {
        const { billNo, type, date, party, paymentMode, amount } = txn;

        const amt = parseFloat(String(amount || "0").replace(/,/g, ""));
        if (isNaN(amt) || amt <= 0) {
          skipped++;
          errors.push({ row: rowNum, reason: `Invalid amount: ${amount}` });
          continue;
        }

        const txnDate = new Date(date);
        if (isNaN(txnDate.getTime())) {
          skipped++;
          errors.push({ row: rowNum, reason: `Invalid date: ${date}` });
          continue;
        }

        // Clean party name
        const rawParty = String(party || "").trim();
        if (!rawParty) {
          skipped++;
          errors.push({ row: rowNum, reason: "Missing party" });
          continue;
        }

        const { matched: cleanedParty } = matchPartyName(rawParty, partyNames);
        if (!cleanedParty) {
          skipped++;
          continue;
        }

        // Find or create party
        const normalizedName = normalizePartyKey(cleanedParty);
        let partyId = partyCache.get(normalizedName);

        if (!partyId) {
          const existing = await prisma.party.findFirst({
            where: { normalizedName, companyId },
          });

          if (existing) {
            partyId = existing.partyId;
            partyCache.set(normalizedName, partyId);
          } else {
            const creditAllowed = paymentMode === "Credit" || type === "Sale";
            const newParty = await prisma.party.create({
              data: {
                name: cleanedParty,
                normalizedName,
                type: "Customer",
                creditAllowed,
                companyId,
              },
            });
            partyId = newParty.partyId;
            partyCache.set(normalizedName, partyId);
            partyNames.push(cleanedParty);
            partiesCreated++;
          }
        }

        const financialYear = financialYearForDate(txnDate);
        const billTrimmed = String(billNo || "").trim() || null;

        // Duplicate detection
        let existingTxn = null;
        if (billTrimmed) {
          existingTxn = await prisma.transaction.findFirst({
            where: {
              billNo: billTrimmed,
              txnDate,
              txnType: type,
              paymentMode,
              companyId,
            },
          });
        }

        if (existingTxn) {
          await prisma.transaction.update({
            where: { txnId: existingTxn.txnId },
            data: { partyId, amount: amt, financialYear },
          });
          updated++;
        } else {
          await prisma.transaction.create({
            data: {
              txnDate,
              billNo: billTrimmed,
              partyId,
              txnType: type,
              paymentMode,
              financialYear,
              amount: amt,
              companyId,
            },
          });

          // Track stats
          if (type === "Receipt") {
            receipts++;
            totalReceipts += amt;
          } else if (paymentMode === "Cash") {
            cashSales++;
            totalCashSales += amt;
          } else if (paymentMode === "Credit") {
            creditSales++;
            totalCreditSales += amt;
          } else {
            upiSales++;
            totalUpiSales += amt;
          }
        }
      } catch (e) {
        skipped++;
        errors.push({ row: rowNum, reason: String(e).slice(0, 80) });
      }
    }

    return NextResponse.json({
      status: "Smart import complete",
      stats: {
        receipts,
        cashSales,
        creditSales,
        upiSales,
        updated,
        skipped,
        partiesCreated,
        totalReceipts,
        totalCashSales,
        totalCreditSales,
        totalUpiSales,
      },
      total: transactions.length,
      errors,
    });
  } catch (error) {
    console.error("POST /api/import/smart error:", error);
    return NextResponse.json({ error: "Smart import failed" }, { status: 500 });
  }
}
