import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { getCurrentFinancialYear } from "@/lib/financial-year";

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "";

    // Get distinct financial years from transactions
    const results = await prisma.transaction.findMany({
      where: companyId ? { companyId } : {},
      select: { financialYear: true },
      distinct: ["financialYear"],
      orderBy: { financialYear: "desc" },
    });

    const years = results.map((r) => r.financialYear).filter(Boolean);
    const currentFY = getCurrentFinancialYear();

    // Ensure current FY is always in the list
    const merged = [...new Set([currentFY, ...years])].sort().reverse();

    // Get the selected FY from settings
    let selected = currentFY;
    if (companyId) {
      const setting = await prisma.appSetting.findFirst({
        where: { settingKey: "active_financial_year", companyId },
      });
      if (setting?.settingValue) {
        selected = setting.settingValue;
      }
    }

    return NextResponse.json({ years: merged, selected });
  } catch (error) {
    console.error("GET /api/financial-years error:", error);
    return NextResponse.json({ error: "Failed to fetch financial years" }, { status: 500 });
  }
}
