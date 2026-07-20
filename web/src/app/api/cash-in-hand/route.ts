import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";
    const date = searchParams.get("date") || new Date().toISOString().split("T")[0];

    const record = await prisma.dailyCash.findFirst({
      where: { companyId, cashDate: new Date(date) },
    });

    return NextResponse.json({ date, cashInHand: record ? Number(record.cashInHand) : null });
  } catch (error) {
    console.error("GET /api/cash-in-hand error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}

export async function POST(request: Request) {
  try {
    const { companyId = "cm_default_001", date, cashInHand } = await request.json();

    if (!date || cashInHand === undefined) {
      return NextResponse.json({ error: "date and cashInHand required" }, { status: 400 });
    }

    const cashDate = new Date(date);
    const existing = await prisma.dailyCash.findFirst({
      where: { companyId, cashDate },
    });

    if (existing) {
      await prisma.dailyCash.update({
        where: { id: existing.id },
        data: { cashInHand: parseFloat(cashInHand), updatedAt: new Date() },
      });
    } else {
      await prisma.dailyCash.create({
        data: { cashDate, cashInHand: parseFloat(cashInHand), companyId },
      });
    }

    return NextResponse.json({ status: "Saved", date, cashInHand });
  } catch (error) {
    console.error("POST /api/cash-in-hand error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
