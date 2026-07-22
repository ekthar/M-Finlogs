/**
 * PDF Export Utility
 * Uses jsPDF + jspdf-autotable for client-side PDF generation.
 * Provides branded, formatted PDF reports for inventory and financial data.
 */

import jsPDF from "jspdf";
import autoTable from "jspdf-autotable";

// ── Types ──────────────────────────────────────────────────────────────────

interface ExportPdfOptions {
  title: string;
  subtitle?: string;
  columns: string[];
  rows: (string | number)[][];
  filename?: string;
  orientation?: "portrait" | "landscape";
  meta?: { label: string; value: string }[];
}

interface InventoryPdfOptions {
  title: string;
  products: { name: string; unit: string }[];
  days: number[];
  data: number[][]; // products × days matrix
}

interface LedgerPdfOptions {
  title: string;
  partyName: string;
  balance: number;
  entries: {
    date: string;
    billNo: string;
    txnType: string;
    paymentMode: string;
    debit: number;
    credit: number;
    balance: number;
  }[];
}

// ── Helpers ────────────────────────────────────────────────────────────────

function fmt(n: number): string {
  return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n);
}

function addHeader(doc: jsPDF, title: string, subtitle?: string) {
  const pageWidth = doc.internal.pageSize.getWidth();

  // Brand bar
  doc.setFillColor(24, 24, 27);
  doc.rect(0, 0, pageWidth, 18, "F");

  // Brand text
  doc.setFont("helvetica", "bold");
  doc.setFontSize(11);
  doc.setTextColor(255, 255, 255);
  doc.text("M-Finlogs", 14, 12);

  // Title
  doc.setTextColor(24, 24, 27);
  doc.setFontSize(16);
  doc.setFont("helvetica", "bold");
  doc.text(title, 14, 32);

  // Subtitle
  if (subtitle) {
    doc.setFontSize(10);
    doc.setFont("helvetica", "normal");
    doc.setTextColor(100, 100, 100);
    doc.text(subtitle, 14, 39);
  }

  // Generated timestamp
  doc.setFontSize(8);
  doc.setTextColor(150, 150, 150);
  doc.text(
    `Generated: ${new Date().toLocaleString("en-IN")}`,
    pageWidth - 14,
    32,
    { align: "right" }
  );
}

function addFooter(doc: jsPDF) {
  const pageCount = doc.getNumberOfPages();
  for (let i = 1; i <= pageCount; i++) {
    doc.setPage(i);
    const pageHeight = doc.internal.pageSize.getHeight();
    const pageWidth = doc.internal.pageSize.getWidth();
    doc.setFontSize(8);
    doc.setTextColor(150, 150, 150);
    doc.text(`Page ${i} of ${pageCount}`, pageWidth / 2, pageHeight - 8, {
      align: "center",
    });
  }
}

// ── Generic Table Export ───────────────────────────────────────────────────

export async function exportTablePdf(options: ExportPdfOptions): Promise<void> {
  const {
    title,
    subtitle,
    columns,
    rows,
    filename = "report.pdf",
    orientation = "portrait",
    meta,
  } = options;

  const doc = new jsPDF({ orientation, unit: "mm", format: "a4" });
  addHeader(doc, title, subtitle);

  let startY = subtitle ? 46 : 40;

  // Meta info (key-value pairs above the table)
  if (meta && meta.length > 0) {
    doc.setFontSize(9);
    doc.setFont("helvetica", "normal");
    meta.forEach((m, idx) => {
      doc.setTextColor(100, 100, 100);
      doc.text(`${m.label}:`, 14, startY + idx * 5);
      doc.setTextColor(24, 24, 27);
      doc.text(m.value, 14 + doc.getTextWidth(`${m.label}: `), startY + idx * 5);
    });
    startY += meta.length * 5 + 4;
  }

  autoTable(doc, {
    startY,
    head: [columns],
    body: rows.map((row) => row.map((cell) => String(cell))),
    theme: "grid",
    styles: {
      fontSize: 8.5,
      cellPadding: 2.5,
      lineColor: [228, 228, 231],
      lineWidth: 0.2,
      textColor: [24, 24, 27],
    },
    headStyles: {
      fillColor: [244, 244, 245],
      textColor: [63, 63, 70],
      fontStyle: "bold",
      fontSize: 8,
    },
    alternateRowStyles: {
      fillColor: [250, 250, 250],
    },
    margin: { left: 14, right: 14 },
  });

  addFooter(doc);
  doc.save(filename);
}

// ── Inventory-Specific Export ─────────────────────────────────────────────

export async function exportInventoryPdf(options: InventoryPdfOptions): Promise<void> {
  const { title, products, days, data } = options;

  const doc = new jsPDF({ orientation: "landscape", unit: "mm", format: "a4" });
  addHeader(doc, title, `${products.length} products • ${days.length} days`);

  const columns = ["Product", "Unit", ...days.map(String), "Total"];
  const rows = products.map((product, pIdx) => {
    const rowData = data[pIdx] || new Array(days.length).fill(0);
    const total = rowData.reduce((s: number, v: number) => s + v, 0);
    return [product.name, product.unit, ...rowData.map(String), String(total)];
  });

  autoTable(doc, {
    startY: 44,
    head: [columns],
    body: rows,
    theme: "grid",
    styles: {
      fontSize: 7,
      cellPadding: 1.5,
      lineColor: [228, 228, 231],
      lineWidth: 0.15,
      textColor: [24, 24, 27],
      halign: "center",
    },
    headStyles: {
      fillColor: [244, 244, 245],
      textColor: [63, 63, 70],
      fontStyle: "bold",
      fontSize: 6.5,
    },
    columnStyles: {
      0: { halign: "left", cellWidth: 32 },
      1: { halign: "center", cellWidth: 12 },
    },
    alternateRowStyles: {
      fillColor: [250, 250, 250],
    },
    margin: { left: 8, right: 8 },
  });

  addFooter(doc);

  const filename = title.replace(/[^a-zA-Z0-9]/g, "_").toLowerCase() + ".pdf";
  doc.save(filename);
}

// ── Ledger Export ─────────────────────────────────────────────────────────

export async function exportLedgerPdf(options: LedgerPdfOptions): Promise<void> {
  const { title, partyName, balance, entries } = options;

  const doc = new jsPDF({ orientation: "portrait", unit: "mm", format: "a4" });
  addHeader(doc, title, `Party: ${partyName}`);

  const columns = ["Date", "Bill No", "Type", "Mode", "Debit (₹)", "Credit (₹)", "Balance (₹)"];
  const rows = entries.map((e) => [
    new Date(e.date).toLocaleDateString("en-IN"),
    e.billNo || "—",
    e.txnType,
    e.paymentMode,
    e.debit > 0 ? fmt(e.debit) : "",
    e.credit > 0 ? fmt(e.credit) : "",
    fmt(e.balance),
  ]);

  // Add total row
  rows.push([
    "", "", "", "TOTAL",
    fmt(entries.reduce((s, e) => s + e.debit, 0)),
    fmt(entries.reduce((s, e) => s + e.credit, 0)),
    fmt(balance),
  ]);

  autoTable(doc, {
    startY: 44,
    head: [columns],
    body: rows,
    theme: "grid",
    styles: {
      fontSize: 8.5,
      cellPadding: 2.5,
      lineColor: [228, 228, 231],
      lineWidth: 0.2,
      textColor: [24, 24, 27],
    },
    headStyles: {
      fillColor: [244, 244, 245],
      textColor: [63, 63, 70],
      fontStyle: "bold",
      fontSize: 8,
    },
    alternateRowStyles: {
      fillColor: [250, 250, 250],
    },
    // Style last row (totals) as bold
    didParseCell(data) {
      if (data.row.index === rows.length - 1) {
        data.cell.styles.fontStyle = "bold";
        data.cell.styles.fillColor = [244, 244, 245];
      }
    },
    margin: { left: 14, right: 14 },
  });

  addFooter(doc);

  const filename = `ledger_${partyName.replace(/[^a-zA-Z0-9]/g, "_").toLowerCase()}.pdf`;
  doc.save(filename);
}

// ── Day Book Export ───────────────────────────────────────────────────────

export async function exportDayBookPdf(options: {
  date: string;
  entries: { date: string; billNo: string | null; party: string; txnType: string; paymentMode: string; amount: number }[];
  total: number;
}): Promise<void> {
  const { date, entries, total } = options;

  await exportTablePdf({
    title: "Day Book",
    subtitle: `Date: ${new Date(date).toLocaleDateString("en-IN")}`,
    columns: ["Date", "Bill No", "Party", "Type", "Mode", "Amount (₹)"],
    rows: [
      ...entries.map((e) => [
        new Date(e.date).toLocaleDateString("en-IN"),
        e.billNo || "—",
        e.party,
        e.txnType,
        e.paymentMode,
        fmt(e.amount),
      ]),
      ["", "", "", "", "TOTAL", fmt(total)],
    ],
    filename: `daybook_${date}.pdf`,
  });
}

// ── Daily Summary Export ──────────────────────────────────────────────────

export async function exportDailySummaryPdf(options: {
  rows: { date: string; openingCash: number; cashIn: number; cashExp: number; cashNeeded: number; cashInHand: number; shortExcess: number; bank: number; credit: number; totalSales: number }[];
}): Promise<void> {
  const { rows } = options;

  await exportTablePdf({
    title: "Daily Summary Report",
    subtitle: `${rows.length} days`,
    columns: ["Date", "Opening", "Cash In", "Cash Exp", "Needed", "In Hand", "Short/Excess", "Bank", "Credit", "Total Sales"],
    rows: rows.map((r) => [
      new Date(r.date).toLocaleDateString("en-IN"),
      fmt(r.openingCash),
      fmt(r.cashIn),
      fmt(r.cashExp),
      fmt(r.cashNeeded),
      fmt(r.cashInHand),
      fmt(r.shortExcess),
      fmt(r.bank),
      fmt(r.credit),
      fmt(r.totalSales),
    ]),
    filename: "daily_summary.pdf",
    orientation: "landscape",
  });
}
