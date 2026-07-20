import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { normalizeCompanyKey } from "@/lib/utils";
import { companySchema } from "@/lib/validators";

export async function GET() {
  try {
    const companies = await prisma.company.findMany({
      orderBy: { name: "asc" },
    });
    return NextResponse.json({ companies });
  } catch (error) {
    console.error("GET /api/companies error:", error);
    return NextResponse.json({ error: "Failed to fetch companies" }, { status: 500 });
  }
}

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const parsed = companySchema.safeParse(body);

    if (!parsed.success) {
      return NextResponse.json(
        { error: "Invalid input", details: parsed.error.flatten() },
        { status: 400 }
      );
    }

    const { name } = parsed.data;
    const key = normalizeCompanyKey(name);

    const existing = await prisma.company.findUnique({ where: { key } });
    if (existing) {
      return NextResponse.json({ error: "Company already exists" }, { status: 409 });
    }

    const company = await prisma.company.create({
      data: { name, key },
    });

    return NextResponse.json({ status: "Created", company }, { status: 201 });
  } catch (error) {
    console.error("POST /api/companies error:", error);
    return NextResponse.json({ error: "Failed to create company" }, { status: 500 });
  }
}
