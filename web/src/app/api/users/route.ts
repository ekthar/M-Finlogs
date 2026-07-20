import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { hash } from "argon2";
import { createUserSchema } from "@/lib/validators";

export async function GET() {
  try {
    const users = await prisma.user.findMany({
      select: { username: true, role: true, createdAt: true },
      orderBy: { username: "asc" },
    });
    return NextResponse.json({ users });
  } catch (error) {
    console.error("GET /api/users error:", error);
    return NextResponse.json({ error: "Failed to fetch users" }, { status: 500 });
  }
}

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const parsed = createUserSchema.safeParse(body);

    if (!parsed.success) {
      return NextResponse.json(
        { error: "Invalid input", details: parsed.error.flatten() },
        { status: 400 }
      );
    }

    const { username, password, role } = parsed.data;

    const existing = await prisma.user.findUnique({ where: { username } });
    if (existing) {
      return NextResponse.json({ error: "User already exists" }, { status: 409 });
    }

    const passwordHash = await hash(password, {
      timeCost: 3,
      memoryCost: 65536,
      parallelism: 2,
    });

    const user = await prisma.user.create({
      data: { username, passwordHash, role },
    });

    return NextResponse.json(
      { status: "User Created", user: { username: user.username, role: user.role } },
      { status: 201 }
    );
  } catch (error) {
    console.error("POST /api/users error:", error);
    return NextResponse.json({ error: "Failed to create user" }, { status: 500 });
  }
}
