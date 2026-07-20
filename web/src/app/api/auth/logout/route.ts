import { NextResponse } from "next/server";

export async function POST() {
  const response = NextResponse.json({ status: "ok" });
  response.cookies.set("next-auth.session-token", "", {
    httpOnly: true,
    secure: process.env.NODE_ENV === "production",
    sameSite: "lax",
    maxAge: 0,
    path: "/",
  });
  return response;
}
