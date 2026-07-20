import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "";
    const limit = parseInt(searchParams.get("limit") || "100");

    const logs = await prisma.auditLog.findMany({
      where: companyId ? { companyId } : {},
      orderBy: { timestamp: "desc" },
      take: limit,
    });

    return NextResponse.json({ logs });
  } catch (error) {
    console.error("GET /api/audit error:", error);
    return NextResponse.json({ error: "Failed to fetch audit logs" }, { status: 500 });
  }
}

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const { username, action, details, companyId } = body;

    if (!username || !action) {
      return NextResponse.json({ error: "username and action are required" }, { status: 400 });
    }

    const log = await prisma.auditLog.create({
      data: {
        username,
        action,
        details: details || null,
        companyId: companyId || "",
      },
    });

    return NextResponse.json({ status: "Logged", log }, { status: 201 });
  } catch (error) {
    console.error("POST /api/audit error:", error);
    return NextResponse.json({ error: "Failed to create audit log" }, { status: 500 });
  }
}
