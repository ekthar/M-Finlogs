import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { Prisma } from "@prisma/client";
import { requireAuth } from "@/lib/auth-guard";

/**
 * Trial Balance — Fixed accounting logic:
 * - Sale/Expense/Purchase → Debit (money owed TO us or spent)
 * - Receipt/Sale Return → Credit (money received or returned)
 * - Includes opening balances from parties
 * - Filters by financial year
 */
export async function GET(request: Request) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const financialYear = searchParams.get("financialYear") || "";

    const fyFilter = financialYear ? Prisma.sql`AND t.financial_year = ${financialYear}` : Prisma.sql``;

    const results = await prisma.$queryRaw<Array<{
      name: string;
      type: string;
      debit: number;
      credit: number;
    }>>(Prisma.sql`
      SELECT
        p.name,
        p.type,
        COALESCE(SUM(CASE WHEN t.txn_type IN ('Sale', 'Expense', 'Purchase') THEN t.amount ELSE 0 END), 0) as debit,
        COALESCE(SUM(CASE WHEN t.txn_type IN ('Receipt', 'Sale Return') THEN t.amount ELSE 0 END), 0) as credit
      FROM parties p
      INNER JOIN transactions t ON t.party_id = p.party_id AND t.company_id = p.company_id
      WHERE p.company_id = ${companyId} ${fyFilter}
      GROUP BY p.party_id, p.name, p.type
      HAVING SUM(t.amount) > 0
      ORDER BY p.name
    `);

    // Include opening balances from AppSettings
    const obSettings = await prisma.appSetting.findMany({
      where: { companyId, settingKey: { startsWith: "opening_balance:" } },
    });

    // Get party names for opening balances
    const obPartyIds = obSettings.map(s => parseInt(s.settingKey.replace("opening_balance:", ""))).filter(n => !isNaN(n));
    const obParties = obPartyIds.length > 0 ? await prisma.party.findMany({
      where: { partyId: { in: obPartyIds }, companyId },
      select: { partyId: true, name: true, type: true },
    }) : [];
    const obPartyMap = new Map(obParties.map(p => [p.partyId, p]));

    // Merge opening balances into accounts
    const accountMap = new Map<string, { name: string; type: string; debit: number; credit: number }>();

    for (const r of results) {
      accountMap.set(r.name, {
        name: r.name,
        type: r.type,
        debit: Number(r.debit),
        credit: Number(r.credit),
      });
    }

    for (const ob of obSettings) {
      const pid = parseInt(ob.settingKey.replace("opening_balance:", ""));
      const bal = parseFloat(ob.settingValue || "0");
      if (isNaN(pid) || bal === 0) continue;

      const party = obPartyMap.get(pid);
      if (!party) continue;

      const existing = accountMap.get(party.name);
      if (existing) {
        if (bal > 0) existing.debit += bal;
        else existing.credit += Math.abs(bal);
      } else {
        accountMap.set(party.name, {
          name: party.name,
          type: party.type,
          debit: bal > 0 ? bal : 0,
          credit: bal < 0 ? Math.abs(bal) : 0,
        });
      }
    }

    const accounts = Array.from(accountMap.values()).sort((a, b) => a.name.localeCompare(b.name));
    const totalDebit = accounts.reduce((s, a) => s + a.debit, 0);
    const totalCredit = accounts.reduce((s, a) => s + a.credit, 0);

    return NextResponse.json({ accounts, totalDebit, totalCredit });
  } catch (error) {
    console.error("GET /api/reports/trial-balance error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
