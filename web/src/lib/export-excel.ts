"use client";

import * as XLSX from "xlsx";

/**
 * Export data to Excel (.xlsx)
 * @param data - Array of objects
 * @param filename - Filename without extension
 * @param sheetName - Sheet name
 */
export function exportToExcel(
  data: Record<string, unknown>[],
  filename: string,
  sheetName: string = "Sheet1"
) {
  if (!data || data.length === 0) return;

  const ws = XLSX.utils.json_to_sheet(data);
  const wb = XLSX.utils.book_new();
  XLSX.utils.book_append_sheet(wb, ws, sheetName);

  // Auto-size columns
  const colWidths = Object.keys(data[0]).map((key) => {
    const maxLen = Math.max(
      key.length,
      ...data.map((row) => String(row[key] || "").length)
    );
    return { wch: Math.min(maxLen + 2, 40) };
  });
  ws["!cols"] = colWidths;

  XLSX.writeFile(wb, `${filename}.xlsx`);
}

/**
 * Export table element to Excel
 */
export function exportTableToExcel(tableId: string, filename: string) {
  const table = document.getElementById(tableId);
  if (!table) return;
  const wb = XLSX.utils.table_to_book(table);
  XLSX.writeFile(wb, `${filename}.xlsx`);
}
