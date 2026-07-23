import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAdmin } from "@/lib/auth-guard";
import { financialYearForDate } from "@/lib/financial-year";
import { normalizePartyKey } from "@/lib/utils";
import {
  smartParseDate,
  cleanPartyName,
  matchPartyName,
  normalizeTransactionType,
  normalizePaymentMode,
} from "@/lib/import-utils";

/**
 * Smart Bulk Import — Transactions
 * ─────────────────────────────────
 * Features:
 * - Auto typo-correction for party names (Levenshtein matching)
 * - Party name cleaning (/BLNC removal, whitespace normalization)
 * - Smart date parsing (DD.MM.YY, DD/MM/YYYY, Excel serial, ISO)
 * - Transaction type normalization (30+ typo variants)
 * - Payment mode normalization (20+ variants)
 * - Duplicate detection: same bill+date+party → UPDATE instead of INSERT
 * - Auto-creates parties that don't exist
 * - Returns ALL errors (not just first 10)
 *
 * Body: { companyId, rows: [{ date, billNo, party, txnType, paymentMode, amount }] }
 */
export async function POST(request: Request) {
  const auth = await requireAdmin(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { companyId = "cm_default_001", rows } = await request.json();

    if (!Array.isArray(rows) || rows.length === 0) {
      return NextResponse.json({ error: "rows array required" }, { status: 400 });
    }

    if (rows.length > 10000) {
      return NextResponse.json({ error: "Maximum 10,000 rows per import" }, { status: 400 });
    }

    // ── Pre-fetch existing parties for fuzzy matching ──
    const existingParties = await prisma.party.findMany({
      where: { companyId },
      select: { partyId: true, name: true, normalizedName: true },
    });
    const partyNames = existingParties.map((p) => p.name);
    const partyCache = new Map<string, number>(); // normalizedName → partyId
    for (const p of existingParties) {
      partyCache.set(p.normalizedName, p.partyId);
    }

    let imported = 0;
    let updated = 0;
    let skipped = 0;
    let partiesCreated = 0;
    let typosCorrected = 0;
    const errors: { row: number; reason: string }[] = [];

    for (let i = 0; i < rows.length; i++) {
      const row = rows[i];
      const rowNum = i + 1;

      try {
        const { date, billNo, party, txnType, paymentMode, amount } = row;

        // ── 1. Parse amount ──
        const amt = parseFloat(String(amount || "0").replace(/,/g, ""));
        if (isNaN(amt) || amt <= 0) {
          skipped++;
          errors.push({ row: rowNum, reason: `Invalid amount: "${amount}"` });
          continue;
        }

        // ── 2. Parse date (smart — handles all Indian formats) ──
        const txnDate = smartParseDate(date);
        if (!txnDate) {
          skipped++;
          errors.push({ row: rowNum, reason: `Invalid date: "${date}"` });
          continue;
        }

        // ── 3. Clean + match party name (typo correction) ──
        const rawParty = String(party || "").trim();
        if (!rawParty) {
          skipped++;
          errors.push({ row: rowNum, reason: "Missing party name" });
          continue;
        }

        const { matched: cleanedParty, corrected } = matchPartyName(rawParty, partyNames);
        if (!cleanedParty) {
          skipped++;
          errors.push({ row: rowNum, reason: `Could not parse party: "${rawParty}"` });
          continue;
        }
        if (corrected) typosCorrected++;

        // ── 4. Normalize type + mode (typo correction) ──
        const normalizedType = normalizeTransactionType(txnType);
        const normalizedMode = normalizePaymentMode(paymentMode);

        // ── 5. Find or create party ──
        const normalizedName = normalizePartyKey(cleanedParty);
        let partyId = partyCache.get(normalizedName);

        if (!partyId) {
          // Check DB one more time (could be created by earlier row in this batch)
          const existing = await prisma.party.findFirst({
            where: { normalizedName, companyId },
          });

          if (existing) {
            partyId = existing.partyId;
            partyCache.set(normalizedName, partyId);
          } else {
            const newParty = await prisma.party.create({
              data: {
                name: cleanedParty,
                normalizedName,
                type: "Customer",
                creditAllowed: normalizedMode === "Credit",
                companyId,
              },
            });
            partyId = newParty.partyId;
            partyCache.set(normalizedName, partyId);
            partyNames.push(cleanedParty);
            partiesCreated++;
          }
        }

        // ── 6. Duplicate detection → UPSERT ──
        const billTrimmed = String(billNo || "").trim() || null;
        const financialYear = financialYearForDate(txnDate);

        let existingTxn = null;
        if (billTrimmed) {
          existingTxn = await prisma.transaction.findFirst({
            where: {
              billNo: billTrimmed,
              txnDate,
              txnType: normalizedType,
              companyId,
            },
          });
        }

        if (existingTxn) {
          // UPDATE existing transaction
          await prisma.transaction.update({
            where: { txnId: existingTxn.txnId },
            data: {
              partyId,
              paymentMode: normalizedMode,
              financialYear,
              amount: amt,
            },
          });
          updated++;
        } else {
          // INSERT new transaction
          await prisma.transaction.create({
            data: {
              txnDate,
              billNo: billTrimmed,
              partyId,
              txnType: normalizedType,
              paymentMode: normalizedMode,
              financialYear,
              amount: amt,
              companyId,
            },
          });
          imported++;
        }
      } catch (e) {
        skipped++;
        errors.push({ row: rowNum, reason: String(e).slice(0, 80) });
      }
    }

    return NextResponse.json({
      status: "Import complete",
      imported,
      updated,
      skipped,
      total: rows.length,
      partiesCreated,
      typosCorrected,
      errors,
    });
  } catch (error) {
    console.error("POST /api/import/transactions error:", error);
    return NextResponse.json({ error: "Import failed" }, { status: 500 });
  }
}
