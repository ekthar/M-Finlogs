import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAdmin } from "@/lib/auth-guard";

/**
 * DELETE /api/users/[username] — deactivate a user (set password to null = disabled)
 */
export async function DELETE(request: Request, { params }: { params: Promise<{ username: string }> }) {
  const auth = await requireAdmin(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { username } = await params;

    if (username === "admin") {
      return NextResponse.json({ error: "Cannot deactivate the admin account" }, { status: 400 });
    }

    const user = await prisma.user.findUnique({ where: { username } });
    if (!user) {
      return NextResponse.json({ error: "User not found" }, { status: 404 });
    }

    // Deactivate by setting password to null (can't login)
    await prisma.user.update({
      where: { username },
      data: { passwordHash: null },
    });

    return NextResponse.json({ status: "User deactivated", username });
  } catch (error) {
    console.error("DELETE /api/users/[username] error:", error);
    return NextResponse.json({ error: "Failed" }, { status: 500 });
  }
}
