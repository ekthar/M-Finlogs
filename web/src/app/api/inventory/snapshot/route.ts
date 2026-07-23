import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAuth } from "@/lib/auth-guard";

export async function GET(request: Request) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const financialYear = searchParams.get("financialYear") || "";
    const month = parseInt(searchParams.get("month") || String(new Date().getMonth() + 1));

    // Get items
    const items = await prisma.inventoryItem.findMany({
      where: { companyId },
      orderBy: { id: "asc" },
    });

    if (items.length === 0) {
      return NextResponse.json({ items: [], month, financialYear });
    }

    // Get quantities for this month
    const quantities = await prisma.inventoryQuantity.findMany({
      where: { companyId, financialYear, month },
    });

    // Get purchases for this month
    const purchases = await prisma.inventoryPurchase.findMany({
      where: { companyId, financialYear, month },
    });

    // Build response
    const qtyMap: Record<number, Record<number, number>> = {};
    for (const q of quantities) {
      if (!qtyMap[q.itemId]) qtyMap[q.itemId] = {};
      qtyMap[q.itemId][q.day] = q.qty;
    }

    const purchMap: Record<number, Record<number, number>> = {};
    for (const p of purchases) {
      if (!purchMap[p.itemId]) purchMap[p.itemId] = {};
      purchMap[p.itemId][p.day] = p.qty;
    }

    const result = items.map((item) => {
      const qty = Array.from({ length: 31 }, (_, i) => qtyMap[item.id]?.[i + 1] || 0);
      const purchaseQty = Array.from({ length: 31 }, (_, i) => purchMap[item.id]?.[i + 1] || 0);
      return {
        id: item.clientRowId || `inv-${item.id}`,
        dbId: item.id,
        name: item.name,
        cost: item.cost,
        minStock: item.minStock,
        qty,
        purchaseQty,
      };
    });

    return NextResponse.json({ items: result, month, financialYear });
  } catch (error) {
    console.error("GET /api/inventory/snapshot error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const { companyId = "cm_default_001", financialYear, month, rows } = body;

    if (!financialYear || !month || !rows) {
      return NextResponse.json({ error: "financialYear, month, and rows required" }, { status: 400 });
    }

    for (const row of rows) {
      const { id, name, cost = 0, minStock = 0, qty = [], purchaseQty = [] } = row;
      if (!name?.trim()) continue;

      // Upsert item
      let item = await prisma.inventoryItem.findFirst({
        where: { companyId, clientRowId: id },
      });

      if (!item) {
        item = await prisma.inventoryItem.findFirst({
          where: { companyId, name: { equals: name, mode: "insensitive" } },
        });
      }

      if (item) {
        await prisma.inventoryItem.update({
          where: { id: item.id },
          data: { name, cost, minStock, clientRowId: id, updatedAt: new Date() },
        });
      } else {
        item = await prisma.inventoryItem.create({
          data: { clientRowId: id, name, cost, minStock, companyId },
        });
      }

      // Delete existing quantities for this month and re-insert
      await prisma.inventoryQuantity.deleteMany({
        where: { itemId: item.id, companyId, financialYear, month: parseInt(month) },
      });
      await prisma.inventoryPurchase.deleteMany({
        where: { itemId: item.id, companyId, financialYear, month: parseInt(month) },
      });

      // Insert quantities
      const qtyInserts = (qty as number[])
        .map((q: number, i: number) => ({ itemId: item!.id, companyId, financialYear, month: parseInt(month), day: i + 1, qty: q || 0 }))
        .filter((q) => q.qty !== 0);

      const purchInserts = (purchaseQty as number[])
        .map((q: number, i: number) => ({ itemId: item!.id, companyId, financialYear, month: parseInt(month), day: i + 1, qty: q || 0 }))
        .filter((q) => q.qty !== 0);

      if (qtyInserts.length > 0) {
        await prisma.inventoryQuantity.createMany({ data: qtyInserts });
      }
      if (purchInserts.length > 0) {
        await prisma.inventoryPurchase.createMany({ data: purchInserts });
      }
    }

    return NextResponse.json({ status: "Saved" });
  } catch (error) {
    console.error("POST /api/inventory/snapshot error:", error);
    return NextResponse.json({ error: "Failed to save" }, { status: 500 });
  }
}
