import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";

/**
 * Branding settings — software name, company display name, etc.
 */
export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const companyId = searchParams.get("companyId") || "cm_default_001";

    const settings = await prisma.appSetting.findMany({
      where: { companyId, settingKey: { in: ["software_name", "company_display_name", "company_tagline"] } },
    });

    const result: Record<string, string> = {
      softwareName: "M-Finlogs",
      companyDisplayName: "Default Company",
      companyTagline: "",
    };

    for (const s of settings) {
      if (s.settingKey === "software_name") result.softwareName = s.settingValue || "M-Finlogs";
      if (s.settingKey === "company_display_name") result.companyDisplayName = s.settingValue || "";
      if (s.settingKey === "company_tagline") result.companyTagline = s.settingValue || "";
    }

    return NextResponse.json(result);
  } catch (error) {
    console.error("GET /api/settings/branding error:", error);
    return NextResponse.json({ softwareName: "M-Finlogs", companyDisplayName: "", companyTagline: "" });
  }
}

export async function POST(request: Request) {
  try {
    const { companyId = "cm_default_001", softwareName, companyDisplayName, companyTagline } = await request.json();

    const updates = [
      { key: "software_name", value: softwareName },
      { key: "company_display_name", value: companyDisplayName },
      { key: "company_tagline", value: companyTagline },
    ].filter(u => u.value !== undefined);

    for (const { key, value } of updates) {
      const existing = await prisma.appSetting.findFirst({ where: { settingKey: key, companyId } });
      if (existing) {
        await prisma.appSetting.update({ where: { id: existing.id }, data: { settingValue: value } });
      } else {
        await prisma.appSetting.create({ data: { settingKey: key, settingValue: value, companyId } });
      }
    }

    return NextResponse.json({ status: "Saved" });
  } catch (error) {
    console.error("POST /api/settings/branding error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
