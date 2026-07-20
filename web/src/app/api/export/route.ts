import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAdmin } from "@/lib/auth-guard";

/**
 * Full data export as JSON — admin only.
 * Downloads all transactions, parties, settings, inventory for a company.
 */
export async function GET(request: Request) {
  const auth = await requireAdmin(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";

    const [parties, transactions, settings, inventory, dailyCash, auditLogs] = await Promise.all([
      prisma.party.findMany({ where: { companyId } }),
      prisma.transaction.findMany({ where: { companyId }, orderBy: { txnId: "asc" } }),
      prisma.appSetting.findMany({ where: { companyId } }),
      prisma.inventoryItem.findMany({ where: { companyId }, include: { quantities: true, purchases: true } }),
      prisma.dailyCash.findMany({ where: { companyId } }),
      prisma.auditLog.findMany({ where: { companyId }, orderBy: { timestamp: "desc" }, take: 1000 }),
    ]);

    const exportData = {
      exportedAt: new Date().toISOString(),
      companyId,
      counts: {
        parties: parties.length,
        transactions: transactions.length,
        inventoryItems: inventory.length,
        settings: settings.length,
      },
      parties,
      transactions,
      settings,
      inventory,
      dailyCash,
      auditLogs,
    };

    const json = JSON.stringify(exportData, null, 2);

    return new NextResponse(json, {
      status: 200,
      headers: {
        "Content-Type": "application/json",
        "Content-Disposition": `attachment; filename="mfinlogs_export_${companyId}_${new Date().toISOString().split("T")[0]}.json"`,
      },
    });
  } catch (error) {
    console.error("GET /api/export error:", error);
    return NextResponse.json({ error: "Export failed" }, { status: 500 });
  }
}
