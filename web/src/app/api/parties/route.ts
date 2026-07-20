import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { partySchema } from "@/lib/validators";
import { normalizePartyKey } from "@/lib/utils";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "";

    const parties = await prisma.party.findMany({
      where: companyId ? { companyId } : {},
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
    const parsed = partySchema.safeParse(body);

    if (!parsed.success) {
      return NextResponse.json(
        { error: "Invalid input", details: parsed.error.flatten() },
        { status: 400 }
      );
    }

    const { name, type, creditAllowed } = parsed.data;
    const companyId = body.companyId || "";
    const normalizedName = normalizePartyKey(name);

    // Check for only one Bank
    if (type === "Bank") {
      const existing = await prisma.party.count({
        where: { type: "Bank", companyId },
      });
      if (existing > 0) {
        return NextResponse.json(
          { error: "Only one Bank account is allowed." },
          { status: 400 }
        );
      }
    }

    // Check for duplicate
    const existing = await prisma.party.findFirst({
      where: { normalizedName, companyId },
    });
    if (existing) {
      return NextResponse.json(
        { error: "Party already exists" },
        { status: 409 }
      );
    }

    const party = await prisma.party.create({
      data: {
        name,
        normalizedName,
        type,
        creditAllowed,
        companyId,
      },
    });

    return NextResponse.json({ status: "Party Created", party }, { status: 201 });
  } catch (error) {
    console.error("POST /api/parties error:", error);
    return NextResponse.json({ error: "Failed to create party" }, { status: 500 });
  }
}
