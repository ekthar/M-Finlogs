"use client";

import jsPDF from "jspdf";
import autoTable from "jspdf-autotable";

interface PdfOptions {
  title: string;
  subtitle?: string;
  company?: string;
  headers: string[];
  rows: (string | number)[][];
  filename: string;
  orientation?: "portrait" | "landscape";
}

/**
 * Generate and download a PDF report.
 * Uses jsPDF + autoTable for professional table layouts.
 */
export function exportToPdf({
  title,
  subtitle,
  company = "M-Finlogs",
  headers,
  rows,
  filename,
  orientation = "portrait",
}: PdfOptions) {
  const doc = new jsPDF({ orientation, unit: "mm", format: "a4" });

  const pageWidth = doc.internal.pageSize.getWidth();

  // Header
  doc.setFontSize(16);
  doc.setFont("helvetica", "bold");
  doc.text(company, 14, 15);

  doc.setFontSize(11);
  doc.setFont("helvetica", "normal");
  doc.text(title, 14, 22);

  if (subtitle) {
    doc.setFontSize(9);
    doc.setTextColor(100);
    doc.text(subtitle, 14, 28);
    doc.setTextColor(0);
  }

  // Date on right
  doc.setFontSize(8);
  doc.setTextColor(130);
  const dateStr = new Date().toLocaleDateString("en-IN", { day: "2-digit", month: "short", year: "numeric" });
  doc.text(`Generated: ${dateStr}`, pageWidth - 14, 15, { align: "right" });
  doc.setTextColor(0);

  // Table
  autoTable(doc, {
    startY: subtitle ? 33 : 28,
    head: [headers],
    body: rows,
    theme: "grid",
    headStyles: {
      fillColor: [24, 24, 27],
      textColor: [255, 255, 255],
      fontSize: 8,
      fontStyle: "bold",
      halign: "center",
    },
    bodyStyles: {
      fontSize: 8,
      cellPadding: 2.5,
    },
    alternateRowStyles: {
      fillColor: [248, 250, 252],
    },
    columnStyles: {
      // Right-align amount columns (last few)
      [headers.length - 1]: { halign: "right" },
      [headers.length - 2]: { halign: "right" },
      [headers.length - 3]: { halign: "right" },
    },
    margin: { left: 14, right: 14 },
    didDrawPage: (data) => {
      // Footer with page number
      const pageCount = doc.getNumberOfPages();
      doc.setFontSize(7);
      doc.setTextColor(150);
      doc.text(
        `Page ${data.pageNumber} of ${pageCount}`,
        pageWidth / 2,
        doc.internal.pageSize.getHeight() - 8,
        { align: "center" }
      );
      doc.text(company, 14, doc.internal.pageSize.getHeight() - 8);
    },
  });

  doc.save(`${filename}.pdf`);
}

/**
 * Quick PDF for Ledger
 */
export function exportLedgerPdf(
  partyName: string,
  ledger: { date: string; billNo: string | null; txnType: string; debit: number; credit: number; balance: number }[],
  totalBalance: number
) {
  const fmt = (n: number) => n > 0 ? `₹${n.toLocaleString("en-IN", { minimumFractionDigits: 2 })}` : "";

  const rows = ledger.map((e) => [
    new Date(e.date).toLocaleDateString("en-IN"),
    e.billNo || "—",
    e.txnType,
    fmt(e.debit),
    fmt(e.credit),
    `₹${e.balance.toLocaleString("en-IN", { minimumFractionDigits: 2 })}`,
  ]);

  // Add total row
  rows.push([
    "", "", "TOTAL",
    `₹${ledger.reduce((s, e) => s + e.debit, 0).toLocaleString("en-IN", { minimumFractionDigits: 2 })}`,
    `₹${ledger.reduce((s, e) => s + e.credit, 0).toLocaleString("en-IN", { minimumFractionDigits: 2 })}`,
    `₹${totalBalance.toLocaleString("en-IN", { minimumFractionDigits: 2 })}`,
  ]);

  exportToPdf({
    title: `Party Ledger: ${partyName}`,
    subtitle: `Balance: ₹${Math.abs(totalBalance).toLocaleString("en-IN")} ${totalBalance >= 0 ? "Dr" : "Cr"}`,
    headers: ["Date", "Bill", "Type", "Debit", "Credit", "Balance"],
    rows,
    filename: `Ledger_${partyName.replace(/\s+/g, "_")}`,
  });
}

/**
 * Quick PDF for Outstanding
 */
export function exportOutstandingPdf(
  outstanding: { name: string; status: string; daysUnpaid: number; lastReceipt: string | null; balance: number }[],
  total: number
) {
  const fmt = (n: number) => `₹${n.toLocaleString("en-IN", { minimumFractionDigits: 2 })}`;

  const rows = outstanding.map((e) => [
    e.name,
    e.status,
    String(e.daysUnpaid),
    e.lastReceipt || "Never",
    fmt(e.balance),
  ]);

  rows.push(["", "", "", "TOTAL", fmt(total)]);

  exportToPdf({
    title: "Credit Outstanding Report",
    subtitle: `Total: ${fmt(total)} | Parties: ${outstanding.length}`,
    headers: ["Party", "Status", "Days", "Last Receipt", "Balance"],
    rows,
    filename: "Outstanding_Report",
  });
}
