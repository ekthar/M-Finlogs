"use client";

import { useState, useRef } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Badge } from "@/components/ui/badge";
import { Upload, FileSpreadsheet, CheckCircle, AlertCircle } from "lucide-react";
import { springs } from "@/lib/design-tokens";
import * as XLSX from "xlsx";

interface ParsedRow { date: string; billNo: string; party: string; txnType: string; paymentMode: string; amount: number; }
const itemVariants = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: springs.default } };

export default function ImportPage() {
  const { companyId } = useApp();
  const [rows, setRows] = useState<ParsedRow[]>([]);
  const [fileName, setFileName] = useState("");
  const [importing, setImporting] = useState(false);
  const [result, setResult] = useState<{ imported: number; skipped: number; errors: string[] } | null>(null);
  const fileRef = useRef<HTMLInputElement>(null);

  const handleFile = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    setFileName(file.name);
    setResult(null);

    const reader = new FileReader();
    reader.onload = (evt) => {
      const data = new Uint8Array(evt.target?.result as ArrayBuffer);
      const wb = XLSX.read(data, { type: "array" });
      const ws = wb.Sheets[wb.SheetNames[0]];
      const json = XLSX.utils.sheet_to_json<Record<string, unknown>>(ws);

      // Map columns (flexible: accepts various header names)
      const parsed: ParsedRow[] = json.map((row) => ({
        date: String(row["Date"] || row["date"] || row["txn_date"] || row["TxnDate"] || ""),
        billNo: String(row["Bill"] || row["bill_no"] || row["BillNo"] || row["Bill No"] || ""),
        party: String(row["Party"] || row["party"] || row["Name"] || row["name"] || ""),
        txnType: String(row["Type"] || row["type"] || row["txn_type"] || row["TxnType"] || "Sale"),
        paymentMode: String(row["Mode"] || row["mode"] || row["payment_mode"] || row["PaymentMode"] || "Cash"),
        amount: parseFloat(String(row["Amount"] || row["amount"] || 0)) || 0,
      })).filter(r => r.party && r.amount > 0);

      setRows(parsed);
      toast.success(`Parsed ${parsed.length} rows from ${file.name}`);
    };
    reader.readAsArrayBuffer(file);
  };

  const handleImport = async () => {
    if (rows.length === 0) { toast.error("No rows to import"); return; }
    setImporting(true);
    try {
      const res = await fetch("/api/import/transactions", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ companyId, rows }),
      });
      const data = await res.json();
      setResult(data);
      if (data.imported > 0) toast.success(`${data.imported} transactions imported`);
      if (data.skipped > 0) toast.warning(`${data.skipped} rows skipped`);
    } catch { toast.error("Import failed"); }
    setImporting(false);
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5 max-w-3xl">
      <motion.div variants={itemVariants}><h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-100">Import Transactions</h1><p className="mt-0.5 text-sm text-zinc-500">Upload Excel/CSV to bulk-import transactions</p></motion.div>

      {/* Upload */}
      <motion.div variants={itemVariants}><Card><CardHeader><CardTitle className="flex items-center gap-2"><Upload className="h-4 w-4"/>Upload File</CardTitle></CardHeader><CardContent>
        <div className="border-2 border-dashed border-zinc-200 dark:border-zinc-700 rounded-xl p-8 text-center cursor-pointer hover:border-zinc-400 transition-colors" onClick={() => fileRef.current?.click()}>
          <FileSpreadsheet className="h-10 w-10 mx-auto text-zinc-400 mb-3"/>
          <p className="text-sm font-medium text-zinc-700 dark:text-zinc-300">{fileName || "Click to select .xlsx or .csv"}</p>
          <p className="text-[11px] text-zinc-400 mt-1">Columns: Date, Bill, Party, Type, Mode, Amount</p>
        </div>
        <input ref={fileRef} type="file" accept=".xlsx,.xls,.csv" className="hidden" onChange={handleFile}/>
      </CardContent></Card></motion.div>

      {/* Preview */}
      {rows.length > 0 && (
        <motion.div variants={itemVariants}><Card><CardHeader><CardTitle>Preview ({rows.length} rows)</CardTitle></CardHeader><CardContent>
          <div className="max-h-60 overflow-auto text-xs">
            <table className="w-full"><thead><tr className="bg-zinc-50 dark:bg-zinc-800"><th className="px-2 py-1 text-left">Date</th><th className="px-2 py-1 text-left">Bill</th><th className="px-2 py-1 text-left">Party</th><th className="px-2 py-1 text-left">Type</th><th className="px-2 py-1 text-left">Mode</th><th className="px-2 py-1 text-right">Amount</th></tr></thead><tbody>
              {rows.slice(0, 10).map((r, i) => <tr key={i} className="border-t border-zinc-100 dark:border-zinc-800"><td className="px-2 py-1">{r.date}</td><td className="px-2 py-1">{r.billNo}</td><td className="px-2 py-1">{r.party}</td><td className="px-2 py-1">{r.txnType}</td><td className="px-2 py-1">{r.paymentMode}</td><td className="px-2 py-1 text-right font-mono">{r.amount}</td></tr>)}
              {rows.length > 10 && <tr><td colSpan={6} className="px-2 py-1 text-center text-zinc-400">...and {rows.length - 10} more</td></tr>}
            </tbody></table>
          </div>
          <Button className="w-full mt-4" onClick={handleImport} disabled={importing}>{importing ? "Importing..." : `Import ${rows.length} Transactions`}</Button>
        </CardContent></Card></motion.div>
      )}

      {/* Result */}
      {result && (
        <motion.div variants={itemVariants}><Card><CardContent className="py-4">
          <div className="flex items-center gap-4">
            <CheckCircle className="h-8 w-8 text-emerald-500"/>
            <div>
              <p className="text-sm font-medium text-zinc-900 dark:text-zinc-100">Import Complete</p>
              <p className="text-xs text-zinc-500">{result.imported} imported • {result.skipped} skipped</p>
            </div>
          </div>
          {result.errors.length > 0 && (
            <div className="mt-3 rounded-lg bg-red-50 dark:bg-red-900/10 p-3"><p className="text-xs font-medium text-red-700 dark:text-red-400 mb-1">Errors:</p>{result.errors.map((e, i) => <p key={i} className="text-[11px] text-red-600 dark:text-red-400">{e}</p>)}</div>
          )}
        </CardContent></Card></motion.div>
      )}
    </motion.div>
  );
}
