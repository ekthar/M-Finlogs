import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAdmin } from "@/lib/auth-guard";

/**
 * Backup Scheduling (#49)
 * ───────────────────────
 * - GET: Returns backup config + last backup info
 * - POST: Trigger manual backup (full JSON export)
 * - PUT: Update backup schedule settings
 *
 * Backup includes: transactions, parties, inventory, settings, audit logs
 * Stored as JSON download (client-side) or saved schedule in AppSettings.
 */

export async function GET(request: Request) {
  const auth = await requireAdmin(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";

    // Get backup schedule settings
    const scheduleSetting = await prisma.appSetting.findFirst({
      where: { companyId, settingKey: "backup_schedule" },
    });
    const lastBackupSetting = await prisma.appSetting.findFirst({
      where: { companyId, settingKey: "last_backup" },
    });

    const schedule = scheduleSetting ? JSON.parse(scheduleSetting.settingValue || "{}") : {
      enabled: false,
      frequency: "weekly", // daily, weekly, monthly
      dayOfWeek: 0, // 0 = Sunday
      time: "02:00",
    };

    const lastBackup = lastBackupSetting ? JSON.parse(lastBackupSetting.settingValue || "{}") : null;

    // Get data counts for info
    const [txnCount, partyCount, inventoryCount] = await Promise.all([
      prisma.transaction.count({ where: { companyId } }),
      prisma.party.count({ where: { companyId } }),
      prisma.inventoryItem.count({ where: { companyId } }),
    ]);

    return NextResponse.json({
      schedule,
      lastBackup,
      dataSize: { transactions: txnCount, parties: partyCount, inventory: inventoryCount },
    });
  } catch (error) {
    console.error("GET /api/backup/schedule error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}

/**
 * Trigger manual backup — returns full JSON export
 */
export async function POST(request: Request) {
  const auth = await requireAdmin(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { companyId = "cm_default_001" } = await request.json();

    // ── Export all data ──
    const [transactions, parties, inventory, settings, auditLogs, dailyCash] = await Promise.all([
      prisma.transaction.findMany({ where: { companyId }, orderBy: { txnId: "asc" } }),
      prisma.party.findMany({ where: { companyId } }),
      prisma.inventoryItem.findMany({ where: { companyId } }),
      prisma.appSetting.findMany({ where: { companyId } }),
      prisma.auditLog.findMany({ where: { companyId }, orderBy: { timestamp: "desc" }, take: 5000 }),
      prisma.dailyCash.findMany({ where: { companyId } }),
    ]);

    // Get inventory quantities
    const quantities = await prisma.inventoryQuantity.findMany({ where: { companyId } });
    const purchases = await prisma.inventoryPurchase.findMany({ where: { companyId } });

    const backup = {
      metadata: {
        version: "2.0",
        exportedAt: new Date().toISOString(),
        companyId,
        counts: {
          transactions: transactions.length,
          parties: parties.length,
          inventoryItems: inventory.length,
          settings: settings.length,
          auditLogs: auditLogs.length,
        },
      },
      data: {
        transactions,
        parties,
        inventory,
        inventoryQuantities: quantities,
        inventoryPurchases: purchases,
        settings,
        dailyCash,
        auditLogs,
      },
    };

    // Record backup timestamp
    const settingKey = "last_backup";
    await prisma.appSetting.upsert({
      where: { settingKey_companyId: { settingKey, companyId } },
      update: { settingValue: JSON.stringify({ timestamp: new Date().toISOString(), size: JSON.stringify(backup).length, txnCount: transactions.length }) },
      create: { settingKey, settingValue: JSON.stringify({ timestamp: new Date().toISOString(), size: JSON.stringify(backup).length, txnCount: transactions.length }), companyId },
    });

    // Audit log
    try {
      const username = (auth as any).username || "admin";
      await prisma.auditLog.create({
        data: { username, action: "BACKUP_CREATED", details: `Manual backup: ${transactions.length} txns, ${parties.length} parties`, companyId },
      });
    } catch {}

    return NextResponse.json(backup);
  } catch (error) {
    console.error("POST /api/backup/schedule error:", error);
    return NextResponse.json({ error: "Backup failed" }, { status: 500 });
  }
}

/**
 * Update backup schedule settings
 */
export async function PUT(request: Request) {
  const auth = await requireAdmin(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { companyId = "cm_default_001", enabled, frequency, dayOfWeek, time } = await request.json();

    const schedule = {
      enabled: Boolean(enabled),
      frequency: frequency || "weekly",
      dayOfWeek: dayOfWeek ?? 0,
      time: time || "02:00",
      updatedAt: new Date().toISOString(),
    };

    const settingKey = "backup_schedule";
    await prisma.appSetting.upsert({
      where: { settingKey_companyId: { settingKey, companyId } },
      update: { settingValue: JSON.stringify(schedule) },
      create: { settingKey, settingValue: JSON.stringify(schedule), companyId },
    });

    return NextResponse.json({ status: "Schedule updated", schedule });
  } catch (error) {
    console.error("PUT /api/backup/schedule error:", error);
    return NextResponse.json({ error: "Failed to update schedule" }, { status: 500 });
  }
}
