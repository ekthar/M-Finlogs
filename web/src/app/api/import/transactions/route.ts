import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAdmin } from "@/lib/auth-guard";
import { financialYearForDate } from "@/lib/financial-year";
import { normalizePartyKey } from "@/lib/utils";

/**
 * Bulk import transactions from Excel/CSV.
 * Body: { companyId, rows: [{ date, billNo, party, txnType, paymentMode, amount }] }
 * Auto-creates parties that don't exist.
 */
export async function POST(request: Request) {
  const auth = await requireAdmin(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { companyId = "cm_default_001", rows } = await request.json();

    if (!Array.isArray(rows) || rows.length === 0) {
      return NextResponse.json({ error: "rows array required" }, { status: 400 });
    }

    if (rows.length > 5000) {
      return NextResponse.json({ error: "Maximum 5000 rows per import" }, { status: 400 });
    }

    let imported = 0;
    let skipped = 0;
    const errors: string[] = [];

    for (let i = 0; i < rows.length; i++) {
      const row = rows[i];
      try {
        const { date, billNo, party, txnType, paymentMode, amount } = row;

        if (!date || !party || !txnType || !paymentMode || !amount) {
          skipped++;
          errors.push(`Row ${i + 1}: missing fields`);
          continue;
        }

        const amt = parseFloat(amount);
        if (isNaN(amt) || amt <= 0) {
          skipped++;
          errors.push(`Row ${i + 1}: invalid amount`);
          continue;
        }

        // Find or create party
        const normalizedName = normalizePartyKey(party);
        let partyRecord = await prisma.party.findFirst({
          where: { normalizedName, companyId },
        });

        if (!partyRecord) {
          partyRecord = await prisma.party.create({
            data: {
              name: String(party).trim(),
              normalizedName,
              type: "Customer",
              creditAllowed: paymentMode === "Credit",
              companyId,
            },
          });
        }

        const txnDate = new Date(date);
        if (isNaN(txnDate.getTime())) {
          skipped++;
          errors.push(`Row ${i + 1}: invalid date`);
          continue;
        }

        const financialYear = financialYearForDate(txnDate);

        await prisma.transaction.create({
          data: {
            txnDate,
            billNo: billNo || null,
            partyId: partyRecord.partyId,
            txnType,
            paymentMode,
            financialYear,
            amount: amt,
            companyId,
          },
        });
        imported++;
      } catch (e) {
        skipped++;
        errors.push(`Row ${i + 1}: ${String(e).slice(0, 50)}`);
      }
    }

    return NextResponse.json({
      status: "Import complete",
      imported,
      skipped,
      total: rows.length,
      errors: errors.slice(0, 10), // Only return first 10 errors
    });
  } catch (error) {
    console.error("POST /api/import/transactions error:", error);
    return NextResponse.json({ error: "Import failed" }, { status: 500 });
  }
}
