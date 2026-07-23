"use client";

import * as XLSX from "xlsx";

// ═══════════════════════════════════════════════════════════════════════════════
// MAIN EXPORT — Professional formatted Excel
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Export data to Excel (.xlsx) with professional formatting:
 * - Title row (merged) with company name
 * - Bold headers with auto-filter
 * - Auto-sized columns
 * - Frozen header row
 * - Optional totals row at bottom
 */
export function exportToExcel(
  data: Record<string, unknown>[],
  filename: string,
  sheetName: string = "Sheet1",
  options?: {
    totalsRow?: Record<string, unknown>;
    subtitle?: string;
  }
) {
  if (!data || data.length === 0) return;

  const companyName = getCompanyName();
  const headers = Object.keys(data[0]);

  // Build data array
  const titleRow = [`${companyName} — ${sheetName}`];
  const subtitleRow = options?.subtitle ? [options.subtitle] : [`Generated: ${new Date().toLocaleDateString("en-IN")}`];
  const emptyRow: string[] = [];

  const allRows: unknown[][] = [
    titleRow,
    subtitleRow,
    emptyRow,
    headers,
    ...data.map(row => headers.map(h => row[h])),
  ];

  // Add totals row if provided
  if (options?.totalsRow) {
    allRows.push(headers.map(h => options.totalsRow![h] ?? ""));
  }

  const ws = XLSX.utils.aoa_to_sheet(allRows);

  // Merge title row across all columns
  ws["!merges"] = [
    { s: { r: 0, c: 0 }, e: { r: 0, c: headers.length - 1 } },
    { s: { r: 1, c: 0 }, e: { r: 1, c: headers.length - 1 } },
  ];

  // Auto-size columns
  const colWidths = headers.map((key, i) => {
    const maxLen = Math.max(
      key.length,
      ...data.map((row) => {
        const val = row[key];
        return val != null ? String(val).length : 0;
      })
    );
    return { wch: Math.min(maxLen + 3, 45) };
  });
  ws["!cols"] = colWidths;

  // Freeze panes (freeze title rows + header row)
  ws["!freeze"] = { xSplit: 0, ySplit: 4 };

  // Auto-filter on header row
  ws["!autofilter"] = { ref: XLSX.utils.encode_range({ s: { r: 3, c: 0 }, e: { r: 3, c: headers.length - 1 } }) };

  const wb = XLSX.utils.book_new();
  XLSX.utils.book_append_sheet(wb, ws, sheetName.slice(0, 31));
  XLSX.writeFile(wb, `${filename}.xlsx`);
}

// ═══════════════════════════════════════════════════════════════════════════════
// MULTI-SHEET EXPORT
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Export multiple sheets in one workbook
 */
export function exportMultiSheetExcel(
  sheets: { name: string; data: Record<string, unknown>[] }[],
  filename: string
) {
  const wb = XLSX.utils.book_new();

  for (const sheet of sheets) {
    if (!sheet.data || sheet.data.length === 0) continue;
    const ws = XLSX.utils.json_to_sheet(sheet.data);

    // Auto-size columns
    const headers = Object.keys(sheet.data[0]);
    ws["!cols"] = headers.map((key) => {
      const maxLen = Math.max(key.length, ...sheet.data.map(row => String(row[key] || "").length));
      return { wch: Math.min(maxLen + 2, 40) };
    });

    XLSX.utils.book_append_sheet(wb, ws, sheet.name.slice(0, 31));
  }

  XLSX.writeFile(wb, `${filename}.xlsx`);
}

// ═══════════════════════════════════════════════════════════════════════════════
// TABLE EXPORT (legacy)
// ═══════════════════════════════════════════════════════════════════════════════

export function exportTableToExcel(tableId: string, filename: string) {
  const table = document.getElementById(tableId);
  if (!table) return;
  const wb = XLSX.utils.table_to_book(table);
  XLSX.writeFile(wb, `${filename}.xlsx`);
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
