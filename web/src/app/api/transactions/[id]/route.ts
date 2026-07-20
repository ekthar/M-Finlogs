import { NextResponse } from "next/server";
import { prisma } from "@/lib/db";
import { requireAuth } from "@/lib/auth-guard";

export async function PUT(request: Request, { params }: { params: Promise<{ id: string }> }) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { id } = await params;
    const txnId = parseInt(id);
    if (isNaN(txnId)) {
      return NextResponse.json({ error: "Invalid transaction ID" }, { status: 400 });
    }

    // Check day lock
    const txn = await prisma.transaction.findUnique({ where: { txnId }, select: { txnDate: true, companyId: true } });
    if (txn) {
      const lockSetting = await prisma.appSetting.findFirst({
        where: { settingKey: "day_lock_date", companyId: txn.companyId },
      });
      if (lockSetting?.settingValue) {
        const lockDate = new Date(lockSetting.settingValue);
        if (new Date(txn.txnDate) <= lockDate) {
          return NextResponse.json({ error: `Cannot edit — day is locked up to ${lockSetting.settingValue}` }, { status: 403 });
        }
      }
    }

    const body = await request.json();
    const { field, value } = body;

    if (!field || value === undefined) {
      return NextResponse.json({ error: "field and value are required" }, { status: 400 });
    }

    const allowedFields: Record<string, string> = {
      amount: "amount",
      bill_no: "billNo",
      txn_date: "txnDate",
      payment_mode: "paymentMode",
      txn_type: "txnType",
    };

    const prismaField = allowedFields[field];
    if (!prismaField) {
      return NextResponse.json({ error: `Cannot edit field: ${field}` }, { status: 400 });
    }

    let updateValue: unknown = value;
    if (field === "amount") updateValue = parseFloat(value);
    if (field === "txn_date") updateValue = new Date(value);

    await prisma.transaction.update({
      where: { txnId },
      data: { [prismaField]: updateValue },
    });

    return NextResponse.json({ status: "Updated" });
  } catch (error) {
    console.error("PUT /api/transactions/[id] error:", error);
    return NextResponse.json({ error: "Failed to update transaction" }, { status: 500 });
  }
}

export async function DELETE(request: Request, { params }: { params: Promise<{ id: string }> }) {
  const auth = await requireAuth(request);
  if (auth instanceof NextResponse) return auth;

  try {
    const { id } = await params;
    const txnId = parseInt(id);
    if (isNaN(txnId)) {
      return NextResponse.json({ error: "Invalid transaction ID" }, { status: 400 });
    }

    // Check day lock
    const txn = await prisma.transaction.findUnique({ where: { txnId }, select: { txnDate: true, companyId: true } });
    if (txn) {
      const lockSetting = await prisma.appSetting.findFirst({
        where: { settingKey: "day_lock_date", companyId: txn.companyId },
      });
      if (lockSetting?.settingValue) {
        const lockDate = new Date(lockSetting.settingValue);
        if (new Date(txn.txnDate) <= lockDate) {
          return NextResponse.json({ error: `Cannot delete — day is locked up to ${lockSetting.settingValue}` }, { status: 403 });
        }
      }
    }

    await prisma.transaction.delete({ where: { txnId } });

    return NextResponse.json({ status: "Deleted" });
  } catch (error) {
    console.error("DELETE /api/transactions/[id] error:", error);
    return NextResponse.json({ error: "Failed to delete transaction" }, { status: 500 });
  }
}
