"use client";

import jsPDF from "jspdf";
import autoTable from "jspdf-autotable";

// ═══════════════════════════════════════════════════════════════════════════════
// TYPES
// ═══════════════════════════════════════════════════════════════════════════════

interface PdfOptions {
  title: string;
  subtitle?: string;
  company?: string;
  headers: string[];
  rows: (string | number)[][];
  filename: string;
  orientation?: "portrait" | "landscape";
  /** Column indices to right-align (auto-detected if not provided) */
  rightAlignCols?: number[];
  /** Add a bold totals row at the bottom */
  totalsRow?: (string | number)[];
  /** Date range to show in header */
  dateRange?: { from?: string; to?: string };
}

interface LedgerPdfOptions {
  partyName: string;
  ledger: { date: string; billNo: string | null; txnType: string; paymentMode?: string; debit: number; credit: number; balance: number }[];
  totalBalance: number;
  openingBalance?: number;
  dateRange?: { from?: string; to?: string };
  company?: string;
}

// ═══════════════════════════════════════════════════════════════════════════════
// HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

function getCompanyName(): string {
  if (typeof window === "undefined") return "M-Finlogs";
  try {
    const stored = localStorage.getItem("mfinlogs_branding");
    if (stored) {
      const parsed = JSON.parse(stored);
      return parsed.softwareName || parsed.companyDisplayName || "M-Finlogs";
    }
  } catch {}
  return "M-Finlogs";
}

function fmt(n: number): string {
  return n > 0 ? `₹${n.toLocaleString("en-IN", { minimumFractionDigits: 2 })}` : "";
}

function fmtAbs(n: number): string {
  return `₹${Math.abs(n).toLocaleString("en-IN", { minimumFractionDigits: 2 })}`;
}

/** Auto-detect which columns should be right-aligned (numbers/currency) */
function detectRightAlignCols(headers: string[], rows: (string | number)[][]): number[] {
  const rightCols: number[] = [];
  for (let col = 0; col < headers.length; col++) {
    const h = headers[col].toLowerCase();
    if (h.includes("amount") || h.includes("debit") || h.includes("credit") ||
        h.includes("balance") || h.includes("total") || h.includes("₹") ||
        h.includes("qty") || h.includes("value") || h.includes("days") ||
        h.includes("opening") || h.includes("cash") || h.includes("bank") ||
        h.includes("short") || h.includes("excess") || h.includes("profit") ||
        h.includes("expense") || h.includes("sales") || h.includes("receipt")) {
      rightCols.push(col);
    }
  }
  return rightCols;
}

// ═══════════════════════════════════════════════════════════════════════════════
// MAIN EXPORT FUNCTION
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Generate and download a professional PDF report.
 * - Dynamic company name from branding settings
 * - Smart column alignment (auto-detects numeric cols)
 * - Optional totals row (bold, highlighted)
 * - Date range shown in header when provided
 * - Page numbers + company footer on every page
 */
export function exportToPdf({
  title,
  subtitle,
  company,
  headers,
  rows,
  filename,
  orientation = "portrait",
  rightAlignCols,
  totalsRow,
  dateRange,
}: PdfOptions) {
  const companyName = company || getCompanyName();
  const doc = new jsPDF({ orientation, unit: "mm", format: "a4" });
  const pageWidth = doc.internal.pageSize.getWidth();

  // ── Header ──
  doc.setFontSize(14);
  doc.setFont("helvetica", "bold");
  doc.text(companyName, 14, 14);

  doc.setFontSize(11);
  doc.setFont("helvetica", "normal");
  doc.text(title, 14, 21);

  let startY = 26;

  if (subtitle) {
    doc.setFontSize(9);
    doc.setTextColor(80);
    doc.text(subtitle, 14, startY);
    doc.setTextColor(0);
    startY += 5;
  }

  if (dateRange && (dateRange.from || dateRange.to)) {
    doc.setFontSize(8);
    doc.setTextColor(100);
    const rangeText = dateRange.from && dateRange.to
      ? `Period: ${dateRange.from} to ${dateRange.to}`
      : dateRange.from ? `From: ${dateRange.from}` : `Up to: ${dateRange.to}`;
    doc.text(rangeText, 14, startY);
    doc.setTextColor(0);
    startY += 5;
  }

  // Generated date on right
  doc.setFontSize(8);
  doc.setTextColor(130);
  doc.text(
    `Generated: ${new Date().toLocaleDateString("en-IN", { day: "2-digit", month: "short", year: "numeric" })}`,
    pageWidth - 14, 14, { align: "right" }
  );
  doc.setTextColor(0);

  // ── Determine alignment ──
  const alignCols = rightAlignCols || detectRightAlignCols(headers, rows);
  const colStyles: Record<number, { halign: "left" | "right" | "center" }> = {};
  for (const col of alignCols) {
    colStyles[col] = { halign: "right" };
  }

  // ── Add totals row to body if provided ──
  const bodyRows = totalsRow ? [...rows, totalsRow] : rows;

  // ── Table ──
  autoTable(doc, {
    startY: startY + 2,
    head: [headers],
    body: bodyRows.map(r => r.map(c => String(c))),
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
    columnStyles: colStyles,
    margin: { left: 14, right: 14 },
    didParseCell(data) {
      // Bold the totals row (last row if totalsRow provided)
      if (totalsRow && data.section === "body" && data.row.index === bodyRows.length - 1) {
        data.cell.styles.fontStyle = "bold";
        data.cell.styles.fillColor = [240, 240, 245];
      }
    },
    didDrawPage: (data) => {
      const pageCount = doc.getNumberOfPages();
      const pageHeight = doc.internal.pageSize.getHeight();
      doc.setFontSize(7);
      doc.setTextColor(150);
      doc.text(`Page ${data.pageNumber} of ${pageCount}`, pageWidth / 2, pageHeight - 8, { align: "center" });
      doc.text(companyName, 14, pageHeight - 8);
      doc.text(new Date().toLocaleDateString("en-IN"), pageWidth - 14, pageHeight - 8, { align: "right" });
    },
  });

  doc.save(`${filename}.pdf`);
}

// ═══════════════════════════════════════════════════════════════════════════════
// LEDGER PDF — with opening balance and date range
// ═══════════════════════════════════════════════════════════════════════════════

export function exportLedgerPdf(
  partyNameOrOpts: string | LedgerPdfOptions,
  ledger?: { date: string; billNo: string | null; txnType: string; debit: number; credit: number; balance: number; paymentMode?: string }[],
  totalBalance?: number
) {
  // Support both old API (3 args) and new API (single options object)
  let opts: LedgerPdfOptions;
  if (typeof partyNameOrOpts === "string") {
    opts = { partyName: partyNameOrOpts, ledger: ledger!, totalBalance: totalBalance! };
  } else {
    opts = partyNameOrOpts;
  }

  const { partyName, totalBalance: bal, openingBalance, dateRange, company } = opts;
  const entries = opts.ledger;
  const totalDebit = entries.reduce((s, e) => s + e.debit, 0);
  const totalCredit = entries.reduce((s, e) => s + e.credit, 0);

  // Build subtitle
  const parts: string[] = [];
  if (openingBalance && openingBalance !== 0) {
    parts.push(`Opening: ${fmtAbs(openingBalance)} ${openingBalance >= 0 ? "Dr" : "Cr"}`);
  }
  parts.push(`Closing: ${fmtAbs(bal)} ${bal >= 0 ? "Dr" : "Cr"}`);
  parts.push(`${entries.length} entries`);

  const rows: (string | number)[][] = entries.map((e) => [
    new Date(e.date).toLocaleDateString("en-IN"),
    e.billNo || "—",
    e.txnType,
    e.debit > 0 ? fmtAbs(e.debit) : "",
    e.credit > 0 ? fmtAbs(e.credit) : "",
    `${fmtAbs(e.balance)} ${e.balance >= 0 ? "Dr" : "Cr"}`,
  ]);

  exportToPdf({
    title: `Party Ledger: ${partyName}`,
    subtitle: parts.join(" | "),
    company,
    headers: ["Date", "Bill No", "Type", "Debit (₹)", "Credit (₹)", "Balance"],
    rows,
    totalsRow: ["", "", "TOTAL", fmtAbs(totalDebit), fmtAbs(totalCredit), `${fmtAbs(bal)} ${bal >= 0 ? "Dr" : "Cr"}`],
    filename: `Ledger_${partyName.replace(/\s+/g, "_")}`,
    dateRange,
    rightAlignCols: [3, 4, 5],
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// OUTSTANDING PDF — handles negative balances
// ═══════════════════════════════════════════════════════════════════════════════

export function exportOutstandingPdf(
  outstanding: { name: string; status: string; daysUnpaid: number; lastReceipt: string | null; balance: number }[],
  total: number
) {
  const receivable = outstanding.filter(e => e.balance > 0);
  const overpaid = outstanding.filter(e => e.balance < 0);
  const totalReceivable = receivable.reduce((s, e) => s + e.balance, 0);
  const totalOverpaid = overpaid.reduce((s, e) => s + e.balance, 0);

  const rows: (string | number)[][] = outstanding.map((e, i) => [
    String(i + 1),
    e.name,
    e.balance < 0 ? "Overpaid" : e.status,
    String(e.daysUnpaid),
    e.lastReceipt || "Never",
    e.balance >= 0 ? fmtAbs(e.balance) : `-${fmtAbs(e.balance)}`,
  ]);

  const subtitle = `Receivable: ${fmtAbs(totalReceivable)} | Overpaid: ${fmtAbs(Math.abs(totalOverpaid))} | Net: ${fmtAbs(total)} | ${outstanding.length} parties`;

  exportToPdf({
    title: "Credit Outstanding Report",
    subtitle,
    headers: ["#", "Party", "Status", "Days", "Last Receipt", "Outstanding (₹)"],
    rows,
    totalsRow: ["", "NET TOTAL", "", "", "", fmtAbs(total)],
    filename: "Outstanding_Report",
    rightAlignCols: [3, 5],
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// TRIAL BALANCE PDF
// ═══════════════════════════════════════════════════════════════════════════════

export function exportTrialBalancePdf(
  accounts: { name: string; type: string; debit: number; credit: number }[],
  totalDebit: number,
  totalCredit: number
) {
  const rows: (string | number)[][] = accounts.map((a, i) => [
    String(i + 1),
    a.name,
    a.type,
    a.debit > 0 ? fmtAbs(a.debit) : "",
    a.credit > 0 ? fmtAbs(a.credit) : "",
  ]);

  exportToPdf({
    title: "Trial Balance",
    subtitle: `Total Debit: ${fmtAbs(totalDebit)} | Total Credit: ${fmtAbs(totalCredit)} | Difference: ${fmtAbs(Math.abs(totalDebit - totalCredit))}`,
    headers: ["#", "Account / Party", "Type", "Debit (₹)", "Credit (₹)"],
    rows,
    totalsRow: ["", "TOTAL", "", fmtAbs(totalDebit), fmtAbs(totalCredit)],
    filename: "Trial_Balance",
    rightAlignCols: [3, 4],
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// P&L PDF
// ═══════════════════════════════════════════════════════════════════════════════

export function exportProfitLossPdf(data: {
  totalSales: number; totalReturns: number; revenue: number;
  totalPurchases: number; grossProfit: number; totalExpenses: number; netProfit: number;
}) {
  const rows: (string | number)[][] = [
    ["Total Sales", fmtAbs(data.totalSales)],
    ["Less: Sale Returns", data.totalReturns > 0 ? `-${fmtAbs(data.totalReturns)}` : "—"],
    ["Revenue (Net Sales)", fmtAbs(data.revenue)],
    ["", ""],
    ["Less: Purchases (COGS)", data.totalPurchases > 0 ? `-${fmtAbs(data.totalPurchases)}` : "—"],
    ["Gross Profit", fmtAbs(data.grossProfit)],
    ["", ""],
    ["Less: Expenses", `-${fmtAbs(data.totalExpenses)}`],
    ["Net Profit / (Loss)", `${data.netProfit >= 0 ? "" : "-"}${fmtAbs(data.netProfit)}`],
  ];

  exportToPdf({
    title: "Profit & Loss Statement",
    subtitle: `Net ${data.netProfit >= 0 ? "Profit" : "Loss"}: ${fmtAbs(data.netProfit)}`,
    headers: ["Particulars", "Amount (₹)"],
    rows,
    filename: "Profit_Loss_Statement",
    rightAlignCols: [1],
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// PARTY STATEMENT PDF (#45)
// ═══════════════════════════════════════════════════════════════════════════════

export function exportPartyStatement(opts: {
  partyName: string;
  openingBalance: number;
  entries: { date: string; billNo: string | null; txnType: string; debit: number; credit: number; balance: number }[];
  closingBalance: number;
  dateRange?: { from?: string; to?: string };
  company?: string;
}) {
  const { partyName, openingBalance, entries, closingBalance, dateRange, company } = opts;

  const rows: (string | number)[][] = [];

  // Opening balance row
  if (openingBalance !== 0) {
    rows.push(["", "", "Opening Balance (B/F)", "", "", `${fmtAbs(openingBalance)} ${openingBalance >= 0 ? "Dr" : "Cr"}`]);
  }

  for (const e of entries) {
    rows.push([
      new Date(e.date).toLocaleDateString("en-IN"),
      e.billNo || "",
      e.txnType,
      e.debit > 0 ? fmtAbs(e.debit) : "",
      e.credit > 0 ? fmtAbs(e.credit) : "",
      `${fmtAbs(e.balance)} ${e.balance >= 0 ? "Dr" : "Cr"}`,
    ]);
  }

  const totalDebit = entries.reduce((s, e) => s + e.debit, 0);
  const totalCredit = entries.reduce((s, e) => s + e.credit, 0);

  exportToPdf({
    title: `Statement of Account: ${partyName}`,
    subtitle: `Closing Balance: ${fmtAbs(closingBalance)} ${closingBalance >= 0 ? "Dr" : "Cr"} | ${entries.length} transactions`,
    company,
    headers: ["Date", "Bill No", "Particulars", "Debit (₹)", "Credit (₹)", "Balance"],
    rows,
    totalsRow: ["", "", "CLOSING BALANCE", fmtAbs(totalDebit), fmtAbs(totalCredit), `${fmtAbs(closingBalance)} ${closingBalance >= 0 ? "Dr" : "Cr"}`],
    filename: `Statement_${partyName.replace(/\s+/g, "_")}`,
    dateRange,
    rightAlignCols: [3, 4, 5],
  });
}
