/**
 * Excel Export Utility
 * Uses SheetJS (xlsx) for client-side Excel generation.
 * Provides formatted .xlsx downloads for inventory and financial data.
 */

import * as XLSX from "xlsx";

// ── Types ──────────────────────────────────────────────────────────────────

interface ExportTableExcelOptions {
  sheetName?: string;
  headers: string[];
  rows: (string | number | null | undefined)[][];
  filename?: string;
}

interface InventoryExcelOptions {
  title: string;
  products: { name: string; unit: string }[];
  days: number[];
  data: number[][]; // products × days matrix
}

// ── Helpers ────────────────────────────────────────────────────────────────

function triggerDownload(workbook: XLSX.WorkBook, filename: string) {
  const wbout = XLSX.write(workbook, { bookType: "xlsx", type: "array" });
  const blob = new Blob([wbout], {
    type: "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
  });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = filename;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(url);
}

function autoFitColumns(ws: XLSX.WorkSheet, data: (string | number | null | undefined)[][]) {
  const colWidths: number[] = [];
  data.forEach((row) => {
    row.forEach((cell, colIdx) => {
      const cellLen = cell != null ? String(cell).length : 0;
      colWidths[colIdx] = Math.max(colWidths[colIdx] || 8, Math.min(cellLen + 2, 40));
    });
  });
  ws["!cols"] = colWidths.map((w) => ({ wch: w }));
}

// ── Generic Table Export ───────────────────────────────────────────────────

export async function exportTableExcel(options: ExportTableExcelOptions): Promise<void> {
  const {
    sheetName = "Report",
    headers,
    rows,
    filename = "report.xlsx",
  } = options;

  const wb = XLSX.utils.book_new();

  // Build data array with header row + title row
  const titleRow = [`M-Finlogs — ${sheetName}`];
  const emptyRow: string[] = [];
  const allData = [titleRow, emptyRow, headers, ...rows];

  const ws = XLSX.utils.aoa_to_sheet(allData);

  // Merge title across all header columns
  ws["!merges"] = [
    { s: { r: 0, c: 0 }, e: { r: 0, c: headers.length - 1 } },
  ];

  // Auto-fit column widths
  autoFitColumns(ws, [headers, ...rows]);

  XLSX.utils.book_append_sheet(wb, ws, sheetName.slice(0, 31)); // Sheet name max 31 chars
  triggerDownload(wb, filename);
}

// ── Inventory-Specific Export ─────────────────────────────────────────────

export async function exportInventoryExcel(options: InventoryExcelOptions): Promise<void> {
  const { title, products, days, data } = options;

  const wb = XLSX.utils.book_new();

  // Header row
  const headers = ["Product", "Unit", ...days.map((d) => `Day ${d}`), "Total"];

  // Data rows
  const rows = products.map((product, pIdx) => {
    const rowData = data[pIdx] || new Array(days.length).fill(0);
    const total = rowData.reduce((s: number, v: number) => s + v, 0);
    return [product.name, product.unit, ...rowData, total];
  });

  // Build sheet
  const titleRow = [`M-Finlogs — ${title}`];
  const generatedRow = [`Generated: ${new Date().toLocaleString("en-IN")}`];
  const emptyRow: string[] = [];
  const allData = [titleRow, generatedRow, emptyRow, headers, ...rows];

  const ws = XLSX.utils.aoa_to_sheet(allData);

  // Merge title row across all columns
  ws["!merges"] = [
    { s: { r: 0, c: 0 }, e: { r: 0, c: headers.length - 1 } },
  ];

  // Set column widths
  const colWidths: number[] = headers.map((h, i) => {
    if (i === 0) return 25; // Product name
    if (i === 1) return 8;  // Unit
    if (i === headers.length - 1) return 10; // Total
    return 6; // Day columns
  });
  ws["!cols"] = colWidths.map((w) => ({ wch: w }));

  // Freeze panes: freeze header row and product column
  ws["!freeze"] = { xSplit: 2, ySplit: 4 };

  XLSX.utils.book_append_sheet(wb, ws, "Inventory");

  const filename = title.replace(/[^a-zA-Z0-9]/g, "_").toLowerCase() + ".xlsx";
  triggerDownload(wb, filename);
}

// ── Ledger Export ─────────────────────────────────────────────────────────

export async function exportLedgerExcel(options: {
  partyName: string;
  entries: { date: string; billNo: string | null; txnType: string; paymentMode: string; debit: number; credit: number; balance: number }[];
  totalBalance: number;
}): Promise<void> {
  const { partyName, entries, totalBalance } = options;

  const headers = ["Date", "Bill No", "Type", "Mode", "Debit (₹)", "Credit (₹)", "Balance (₹)"];
  const rows: (string | number)[][] = entries.map((e) => [
    new Date(e.date).toLocaleDateString("en-IN"),
    e.billNo || "—",
    e.txnType,
    e.paymentMode,
    e.debit,
    e.credit,
    e.balance,
  ]);

  // Total row
  rows.push([
    "", "", "", "TOTAL",
    entries.reduce((s, e) => s + e.debit, 0),
    entries.reduce((s, e) => s + e.credit, 0),
    totalBalance,
  ]);

  await exportTableExcel({
    sheetName: `Ledger - ${partyName}`.slice(0, 31),
    headers,
    rows,
    filename: `ledger_${partyName.replace(/\s+/g, "_").toLowerCase()}.xlsx`,
  });
}

// ── Day Book Export ───────────────────────────────────────────────────────

export async function exportDayBookExcel(options: {
  date: string;
  entries: { date: string; billNo: string | null; party: string; txnType: string; paymentMode: string; amount: number }[];
  total: number;
}): Promise<void> {
  const { date, entries, total } = options;

  const headers = ["Date", "Bill No", "Party", "Type", "Mode", "Amount (₹)"];
  const rows: (string | number)[][] = [
    ...entries.map((e) => [
      new Date(e.date).toLocaleDateString("en-IN"),
      e.billNo || "—",
      e.party,
      e.txnType,
      e.paymentMode,
      e.amount,
    ]),
    ["", "", "", "", "TOTAL", total],
  ];

  await exportTableExcel({
    sheetName: "Day Book",
    headers,
    rows,
    filename: `daybook_${date}.xlsx`,
  });
}

// ── Daily Summary Export ──────────────────────────────────────────────────

export async function exportDailySummaryExcel(options: {
  rows: { date: string; openingCash: number; cashIn: number; cashExp: number; cashNeeded: number; cashInHand: number; shortExcess: number; bank: number; credit: number; totalSales: number }[];
}): Promise<void> {
  const { rows } = options;

  const headers = ["Date", "Opening", "Cash In", "Cash Exp", "Needed", "In Hand", "Short/Excess", "Bank", "Credit", "Total Sales"];
  const dataRows: (string | number)[][] = rows.map((r) => [
    new Date(r.date).toLocaleDateString("en-IN"),
    r.openingCash,
    r.cashIn,
    r.cashExp,
    r.cashNeeded,
    r.cashInHand,
    r.shortExcess,
    r.bank,
    r.credit,
    r.totalSales,
  ]);

  await exportTableExcel({
    sheetName: "Daily Summary",
    headers,
    rows: dataRows,
    filename: "daily_summary.xlsx",
  });
}
