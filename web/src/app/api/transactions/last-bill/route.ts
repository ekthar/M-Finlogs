import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";

    const last = await prisma.transaction.findFirst({
      where: { companyId, billNo: { not: null } },
      orderBy: [{ txnId: "desc" }],
      select: { billNo: true },
    });

    let nextBill = "";
    if (last?.billNo) {
      // Extract trailing number and increment
      const match = last.billNo.match(/(\d+)$/);
      if (match) {
        const num = parseInt(match[1]) + 1;
        const prefix = last.billNo.slice(0, last.billNo.length - match[1].length);
        nextBill = prefix + String(num);
      }
    }

    return NextResponse.json({ lastBill: last?.billNo || "", nextBill });
  } catch (error) {
    console.error("GET /api/transactions/last-bill error:", error);
    return NextResponse.json({ lastBill: "", nextBill: "" }, { status: 200 });
  }
}
