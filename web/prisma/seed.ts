import { PrismaClient } from "@prisma/client";
import bcrypt from "bcryptjs";

const prisma = new PrismaClient();

async function main() {
  // Create default company
  const company = await prisma.company.upsert({
    where: { key: "default" },
    update: {},
    create: {
      name: "Default Company",
      key: "default",
    },
  });

  console.log("✅ Company created:", company.name);

  // Create admin user with default password
  const passwordHash = await bcrypt.hash("admin123", 12);

  const admin = await prisma.user.upsert({
    where: { username: "admin" },
    update: {},
    create: {
      username: "admin",
      passwordHash,
      role: "admin",
    },
  });

  console.log("✅ Admin user created:", admin.username);
  console.log("   ⚠️  Default password: admin123 — CHANGE THIS IMMEDIATELY after first login!");

  // Create default app settings
  await prisma.appSetting.upsert({
    where: {
      settingKey_companyId: {
        settingKey: "company_name",
        companyId: company.id,
      },
    },
    update: {},
    create: {
      settingKey: "company_name",
      settingValue: "Default Company",
      companyId: company.id,
    },
  });

  const now = new Date();
  const startYear = now.getMonth() >= 3 ? now.getFullYear() : now.getFullYear() - 1;
  const currentFY = `${startYear}-${startYear + 1}`;

  await prisma.appSetting.upsert({
    where: {
      settingKey_companyId: {
        settingKey: "active_financial_year",
        companyId: company.id,
      },
    },
    update: {},
    create: {
      settingKey: "active_financial_year",
      settingValue: currentFY,
      companyId: company.id,
    },
  });

  console.log("✅ Financial year set:", currentFY);
  console.log("\n🎉 Database seeded successfully! You can now log in.");
}

main()
  .catch((e) => {
    console.error("❌ Seed failed:", e);
    process.exit(1);
  })
  .finally(async () => {
    await prisma.$disconnect();
  });
