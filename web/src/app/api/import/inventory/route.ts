import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAdmin } from "@/lib/auth-guard";

/**
 * Smart Inventory Sheet Import
 * ────────────────────────────
 * Accepts parsed inventory data from Excel:
 * - Product names in first column
 * - Day-based quantity columns (1, 2, 3... or "1/01", "2/01"...)
 * - Optional cost column
 * - Supports "qty/purchase" format (e.g., "45/10" → qty=45, purchased=10)
 *
 * Body: {
 *   companyId, financialYear, month,
 *   products: [{ name, cost, qty: number[31], purchaseQty: number[31] }]
 * }
 */
export async function POST(request: Request) {
  const auth = await requireAdmin(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { companyId = "cm_default_001", financialYear, month, products } = await request.json();

    if (!Array.isArray(products) || products.length === 0) {
      return NextResponse.json({ error: "products array required" }, { status: 400 });
    }

    if (!financialYear || !month) {
      return NextResponse.json({ error: "financialYear and month required" }, { status: 400 });
    }

    let itemsCreated = 0;
    let itemsUpdated = 0;
    let quantitiesSaved = 0;
    const errors: { product: string; reason: string }[] = [];

    for (const product of products) {
      try {
        const name = String(product.name || "").trim();
        if (!name) continue; // Skip blank rows (gap rows)

        const cost = parseFloat(String(product.cost || "0")) || 0;
        const minStock = parseFloat(String(product.minStock || "0")) || 0;
        const qty: number[] = Array.isArray(product.qty) ? product.qty : Array(31).fill(0);
        const purchaseQty: number[] = Array.isArray(product.purchaseQty) ? product.purchaseQty : Array(31).fill(0);

        // Find or create inventory item
        let item = await prisma.inventoryItem.findFirst({
          where: {
            companyId,
            name: { equals: name, mode: "insensitive" },
          },
        });

        if (!item) {
          item = await prisma.inventoryItem.create({
            data: {
              name,
              cost,
              minStock,
              clientRowId: `inv-import-${Date.now()}-${Math.random().toString(36).slice(2, 6)}`,
              companyId,
            },
          });
          itemsCreated++;
        } else {
          // Update cost/minStock if provided
          if (cost > 0 || minStock > 0) {
            await prisma.inventoryItem.update({
              where: { id: item.id },
              data: {
                ...(cost > 0 ? { cost } : {}),
                ...(minStock > 0 ? { minStock } : {}),
              },
            });
            itemsUpdated++;
          }
        }

        // Save quantities for each day
        for (let day = 1; day <= 31; day++) {
          const qtyVal = qty[day - 1] || 0;
          const purchVal = purchaseQty[day - 1] || 0;

          if (qtyVal > 0) {
            const existingQty = await prisma.inventoryQuantity.findFirst({
              where: { itemId: item.id, companyId, financialYear, month: Number(month), day },
            });
            if (existingQty) {
              await prisma.inventoryQuantity.update({
                where: { id: existingQty.id },
                data: { qty: qtyVal },
              });
            } else {
              await prisma.inventoryQuantity.create({
                data: { itemId: item.id, companyId, financialYear, month: Number(month), day, qty: qtyVal },
              });
            }
            quantitiesSaved++;
          }

          if (purchVal > 0) {
            const existingPurch = await prisma.inventoryPurchase.findFirst({
              where: { itemId: item.id, companyId, financialYear, month: Number(month), day },
            });
            if (existingPurch) {
              await prisma.inventoryPurchase.update({
                where: { id: existingPurch.id },
                data: { qty: purchVal },
              });
            } else {
              await prisma.inventoryPurchase.create({
                data: { itemId: item.id, companyId, financialYear, month: Number(month), day, qty: purchVal },
              });
            }
          }
        }
      } catch (e) {
        errors.push({ product: product.name || "unknown", reason: String(e).slice(0, 80) });
      }
    }

    return NextResponse.json({
      status: "Inventory import complete",
      itemsCreated,
      itemsUpdated,
      quantitiesSaved,
      totalProducts: products.filter((p: any) => String(p.name || "").trim()).length,
      errors,
    });
  } catch (error) {
    console.error("POST /api/import/inventory error:", error);
    return NextResponse.json({ error: "Import failed" }, { status: 500 });
  }
}
