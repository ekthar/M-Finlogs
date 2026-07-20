import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import bcrypt from "bcryptjs";
import { SignJWT } from "jose";
import { checkRateLimit, recordLoginAttempt } from "@/lib/auth-guard";

const secret = new TextEncoder().encode(process.env.JWT_SECRET || "fallback-secret-change-me");

export async function POST(request: Request) {
  try {
    // Rate limiting
    const ip = request.headers.get("x-forwarded-for") || request.headers.get("x-real-ip") || "unknown";
    const rateCheck = checkRateLimit(ip);
    if (!rateCheck.allowed) {
      return NextResponse.json(
        { error: `Too many attempts. Try again in ${rateCheck.retryAfter}s` },
        { status: 429 }
      );
    }

    const body = await request.json();
    const username = (body.username || "").trim();
    const password = body.password || "";

    if (!username || !password) {
      return NextResponse.json({ error: "Username and password are required" }, { status: 400 });
    }

    let user;
    try {
      user = await prisma.user.findUnique({ where: { username } });
    } catch (dbError) {
      console.error("Database connection error:", dbError);
      return NextResponse.json({ error: "Database connection failed." }, { status: 500 });
    }

    if (!user || !user.passwordHash) {
      recordLoginAttempt(ip, false);
      return NextResponse.json({ error: "Invalid credentials" }, { status: 401 });
    }

    const valid = await bcrypt.compare(password, user.passwordHash);
    if (!valid) {
      recordLoginAttempt(ip, false);
      // Log failed attempt
      try {
        await prisma.auditLog.create({
          data: { username, action: "LOGIN_FAILED", details: `Failed login from ${ip}`, companyId: "cm_default_001" },
        });
      } catch {}
      return NextResponse.json({ error: "Invalid credentials" }, { status: 401 });
    }

    // Success
    recordLoginAttempt(ip, true);

    // Audit log the login
    try {
      await prisma.auditLog.create({
        data: { username: user.username, action: "LOGIN", details: `Logged in from ${ip}`, companyId: "cm_default_001" },
      });
    } catch {}

    const token = await new SignJWT({ username: user.username, role: user.role })
      .setProtectedHeader({ alg: "HS256" })
      .setIssuedAt()
      .setExpirationTime("24h")
      .sign(secret);

    const response = NextResponse.json({
      status: "ok",
      user: { username: user.username, role: user.role },
      token,
    });

    response.cookies.set("next-auth.session-token", token, {
      httpOnly: true,
      secure: process.env.NODE_ENV === "production",
      sameSite: "lax",
      maxAge: 24 * 60 * 60,
      path: "/",
    });

    return response;
  } catch (error) {
    console.error("Login error:", error);
    return NextResponse.json({ error: "Internal server error" }, { status: 500 });
  }
}
