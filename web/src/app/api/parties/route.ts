import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { normalizePartyKey } from "@/lib/utils";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";

    const parties = await prisma.party.findMany({
      where: { companyId },
      orderBy: { name: "asc" },
    });

    return NextResponse.json({ parties });
  } catch (error) {
    console.error("GET /api/parties error:", error);
    return NextResponse.json({ error: "Failed to fetch parties" }, { status: 500 });
  }
}

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const { name, type = "Customer", creditAllowed = false, companyId: bodyCompanyId, autoCreate } = body;
    const companyId = bodyCompanyId || "cm_default_001";

    if (!name?.trim()) {
      return NextResponse.json({ error: "Party name is required" }, { status: 400 });
    }

    const normalizedName = normalizePartyKey(name.trim());

    // Check for only one Bank
    if (type === "Bank") {
      const existing = await prisma.party.count({ where: { type: "Bank", companyId } });
      if (existing > 0) {
        return NextResponse.json({ error: "Only one Bank account is allowed." }, { status: 400 });
      }
    }

    // Check for duplicate
    const existing = await prisma.party.findFirst({
      where: { normalizedName, companyId },
    });
    if (existing) {
      if (autoCreate) {
        // Auto-create mode: return existing party (no error)
        return NextResponse.json({ status: "Exists", party: existing });
      }
      return NextResponse.json({ error: "Party already exists" }, { status: 409 });
    }

    const party = await prisma.party.create({
      data: { name: name.trim(), normalizedName, type, creditAllowed, companyId },
    });

    return NextResponse.json({ status: "Party Created", party }, { status: 201 });
  } catch (error) {
    console.error("POST /api/parties error:", error);
    return NextResponse.json({ error: "Failed to create party" }, { status: 500 });
  }
}
