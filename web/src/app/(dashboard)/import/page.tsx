"use client";

import { useState, useRef, useCallback } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { Select } from "@/components/ui/select";
import {
  Upload, FileSpreadsheet, CheckCircle, AlertTriangle,
  Zap, Package, FileText, ArrowRight, RotateCcw, X,
} from "lucide-react";
import { springs } from "@/lib/design-tokens";
import * as XLSX from "xlsx";
import {
  detectColumns,
  isDailyRegisterFormat,
  parseDailyRegister,
  smartParseDate,
  cleanPartyName,
  normalizeTransactionType,
  normalizePaymentMode,
} from "@/lib/import-utils";


// ── Types ──────────────────────────────────────────────────────────────────
type ImportMode = "idle" | "processing" | "done";
type DetectedFormat = "transactions" | "inventory" | "smart" | "unknown";

interface ImportResult {
  format: DetectedFormat;
  imported?: number;
  updated?: number;
  skipped?: number;
  total?: number;
  partiesCreated?: number;
  typosCorrected?: number;
  itemsCreated?: number;
  quantitiesSaved?: number;
  stats?: Record<string, number>;
  errors?: { row: number; reason: string }[];
}

const itemVariants = {
  initial: { opacity: 0, y: 12 },
  animate: { opacity: 1, y: 0, transition: springs.default },
};

const MONTHS = [
  "January","February","March","April","May","June",
  "July","August","September","October","November","December",
];


// ── Helper: Parse inventory sheet ──────────────────────────────────────────
function parseInventorySheet(rows: unknown[][]) {
  if (!rows || rows.length < 2) return null;
  const header = (rows[0] as string[]).map((h) => String(h || "").trim());

  // Detect product column
  let productCol = header.findIndex((h) => {
    const k = h.toLowerCase();
    return k === "product name" || k === "product" || k === "name" || k === "item" || k === "item name";
  });
  if (productCol < 0) productCol = 0;

  // Detect cost column
  const costCol = header.findIndex((h) => {
    const k = h.toLowerCase();
    return k === "cost" || k === "unit cost" || k === "price";
  });

  // Detect day columns (numbers 1-31 or "1/01" patterns)
  const dayColumns: { col: number; day: number }[] = [];
  header.forEach((h, idx) => {
    if (idx === productCol || idx === costCol) return;
    const m = /^(\d{1,2})(?:[.\/-]\d{1,2})?$/.exec(h);
    if (m) {
      const day = parseInt(m[1]);
      if (day >= 1 && day <= 31) dayColumns.push({ col: idx, day });
    }
  });

  if (dayColumns.length === 0) return null;

  // Detect month from header
  let detectedMonth = new Date().getMonth() + 1;
  const monthHeader = header.find((h) => /^\d{1,2}[.\/-](\d{1,2})$/.test(h));
  if (monthHeader) {
    const mMatch = /[.\/-](\d{1,2})$/.exec(monthHeader);
    if (mMatch) detectedMonth = parseInt(mMatch[1]);
  }


  const products: { name: string; cost: number; qty: number[]; purchaseQty: number[] }[] = [];
  for (let i = 1; i < rows.length; i++) {
    const row = rows[i] as string[];
    const name = String(row?.[productCol] || "").trim();
    if (!name) {
      products.push({ name: "", cost: 0, qty: Array(31).fill(0), purchaseQty: Array(31).fill(0) });
      continue;
    }

    const cost = costCol >= 0 ? (parseFloat(String(row[costCol] || "0").replace(/,/g, "")) || 0) : 0;
    const qty = Array(31).fill(0);
    const purchaseQty = Array(31).fill(0);

    for (const { col, day } of dayColumns) {
      const raw = String(row?.[col] || "").replace(/,/g, "").trim();
      if (!raw) continue;
      const parts = raw.split(/[\/|+]/).map((p) => parseFloat(p.trim()));
      if (Number.isFinite(parts[0])) qty[day - 1] = parts[0];
      if (parts.length > 1 && Number.isFinite(parts[1]) && parts[1] > 0) {
        purchaseQty[day - 1] = parts[1];
      }
    }
    products.push({ name, cost, qty, purchaseQty });
  }

  return { products, detectedMonth, dayCount: dayColumns.length };
}

// ── Helper: Detect format ──────────────────────────────────────────────────
function detectFormat(rawRows: unknown[][]): DetectedFormat {
  if (isDailyRegisterFormat(rawRows)) return "smart";

  // Check if it looks like inventory (day-number columns)
  if (rawRows.length > 0) {
    const header = (rawRows[0] as string[]).map((h) => String(h || "").trim());
    const dayColCount = header.filter((h) => /^\d{1,2}(?:[.\/-]\d{1,2})?$/.test(h)).length;
    if (dayColCount >= 5) return "inventory";
  }

  return "transactions";
}


// ── Main Component ─────────────────────────────────────────────────────────
export default function ImportPage() {
  const { companyId, financialYear } = useApp();
  const [mode, setMode] = useState<ImportMode>("idle");
  const [result, setResult] = useState<ImportResult | null>(null);
  const [detectedFormat, setDetectedFormat] = useState<DetectedFormat>("unknown");
  const [fileName, setFileName] = useState("");
  const [rowCount, setRowCount] = useState(0);
  const [inventoryMonth, setInventoryMonth] = useState(new Date().getMonth() + 1);
  const [dragOver, setDragOver] = useState(false);
  const fileRef = useRef<HTMLInputElement>(null);

  // ── No-questions-asked: auto-detect and import immediately ──
  const processFile = useCallback(async (file: File) => {
    setFileName(file.name);
    setMode("processing");
    setResult(null);

    try {
      const data = new Uint8Array(await file.arrayBuffer());
      const wb = XLSX.read(data, { type: "array", cellDates: false });
      const ws = wb.Sheets[wb.SheetNames[0]];
      const rawRows = XLSX.utils.sheet_to_json<unknown[]>(ws, { header: 1, defval: "" });

      if (!rawRows || rawRows.length < 2) {
        toast.error("File is empty or has no data rows");
        setMode("idle");
        return;
      }

      const format = detectFormat(rawRows);
      setDetectedFormat(format);

      if (format === "smart") {
        // ── Smart Import (Daily Register) ──
        const transactions = parseDailyRegister(rawRows);
        setRowCount(transactions.length);
        toast.info(`Smart format detected — ${transactions.length} transactions`);

        const serialized = transactions.map((t) => ({
          ...t, date: t.date.toISOString(),
        }));

        const res = await fetch("/api/import/smart", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ companyId, transactions: serialized }),
        });
        const data = await res.json();
        setResult({ format, ...data });
        toast.success("Smart import complete!");


      } else if (format === "inventory") {
        // ── Inventory Sheet Import ──
        const parsed = parseInventorySheet(rawRows);
        if (!parsed || parsed.products.length === 0) {
          toast.error("Could not parse inventory data");
          setMode("idle");
          return;
        }
        setRowCount(parsed.products.filter((p) => p.name).length);
        setInventoryMonth(parsed.detectedMonth);
        toast.info(`Inventory sheet — ${parsed.products.filter((p) => p.name).length} products, ${parsed.dayCount} days`);

        const res = await fetch("/api/import/inventory", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            companyId,
            financialYear,
            month: parsed.detectedMonth,
            products: parsed.products,
          }),
        });
        const data = await res.json();
        setResult({ format, ...data });
        toast.success("Inventory imported!");

      } else {
        // ── Standard Transaction Import ──
        const json = XLSX.utils.sheet_to_json<Record<string, unknown>>(ws);
        const headers = rawRows[0] as string[];
        const mapping = detectColumns(headers.map((h) => String(h || "")));

        const rows = json.map((row) => {
          const keys = Object.keys(row);
          return {
            date: String(row[keys[mapping.date]] || row["Date"] || row["date"] || row["txn_date"] || ""),
            billNo: String(row[keys[mapping.billNo]] || row["Bill"] || row["bill_no"] || row["Bill No"] || ""),
            party: String(row[keys[mapping.party]] || row["Party"] || row["party"] || row["Name"] || ""),
            txnType: String(row[keys[mapping.txnType]] || row["Type"] || row["type"] || "Sale"),
            paymentMode: String(row[keys[mapping.paymentMode]] || row["Mode"] || row["mode"] || "Cash"),
            amount: String(row[keys[mapping.amount]] || row["Amount"] || row["amount"] || "0"),
          };
        }).filter((r) => r.party && parseFloat(String(r.amount).replace(/,/g, "")) > 0);

        setRowCount(rows.length);
        toast.info(`${rows.length} transactions detected — importing...`);

        const res = await fetch("/api/import/transactions", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ companyId, rows }),
        });
        const data = await res.json();
        setResult({ format, ...data });
        if (data.imported > 0) toast.success(`${data.imported} transactions imported!`);
      }
    } catch (err) {
      toast.error("Import failed: " + String(err).slice(0, 100));
    }
    setMode("done");
  }, [companyId, financialYear]);


  // ── Event handlers ──
  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (file) processFile(file);
    if (e.target) e.target.value = "";
  };

  const handleDrop = (e: React.DragEvent) => {
    e.preventDefault();
    setDragOver(false);
    const file = e.dataTransfer.files[0];
    if (file) processFile(file);
  };

  const reset = () => {
    setMode("idle");
    setResult(null);
    setDetectedFormat("unknown");
    setFileName("");
    setRowCount(0);
  };

  // ── Render ──
  return (
    <motion.div initial="initial" animate="animate" className="space-y-5 max-w-4xl mx-auto">
      {/* Header */}
      <motion.div variants={itemVariants}>
        <h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-50">Smart Import</h1>
        <p className="mt-1 text-sm text-zinc-600 dark:text-zinc-300">
          Drop any file — auto-detects format, fixes typos, handles duplicates
        </p>
      </motion.div>

      {/* Features badges */}
      <motion.div variants={itemVariants} className="flex flex-wrap gap-2">
        <Badge variant="info">Auto-detect format</Badge>
        <Badge variant="info">Typo correction</Badge>
        <Badge variant="info">Duplicate detection</Badge>
        <Badge variant="info">Party name cleaning</Badge>
        <Badge variant="info">Indian date formats</Badge>
        <Badge variant="info">Smart daily register</Badge>
      </motion.div>


      {/* Upload Zone */}
      {mode === "idle" && (
        <motion.div variants={itemVariants}>
          <Card>
            <CardContent className="p-0">
              <div
                className={`relative border-2 border-dashed rounded-2xl p-12 text-center cursor-pointer transition-all duration-200 ${
                  dragOver
                    ? "border-blue-400 bg-blue-50/50 dark:bg-blue-900/10"
                    : "border-zinc-200 dark:border-zinc-700 hover:border-zinc-400 dark:hover:border-zinc-500"
                }`}
                onClick={() => fileRef.current?.click()}
                onDragOver={(e) => { e.preventDefault(); setDragOver(true); }}
                onDragLeave={() => setDragOver(false)}
                onDrop={handleDrop}
              >
                <div className="flex flex-col items-center gap-4">
                  <div className="flex h-16 w-16 items-center justify-center rounded-2xl bg-zinc-100 dark:bg-zinc-800">
                    <Upload className="h-7 w-7 text-zinc-500 dark:text-zinc-400" />
                  </div>
                  <div>
                    <p className="text-base font-semibold text-zinc-800 dark:text-zinc-200">
                      Drop file here or click to browse
                    </p>
                    <p className="mt-1.5 text-sm text-zinc-500 dark:text-zinc-400">
                      Accepts .xlsx, .xls, .csv — imports instantly
                    </p>
                  </div>
                  <div className="flex flex-wrap justify-center gap-3 mt-2">
                    <div className="flex items-center gap-1.5 text-xs text-zinc-500">
                      <FileText className="h-3.5 w-3.5" /> Transactions
                    </div>
                    <div className="flex items-center gap-1.5 text-xs text-zinc-500">
                      <Package className="h-3.5 w-3.5" /> Inventory sheets
                    </div>
                    <div className="flex items-center gap-1.5 text-xs text-zinc-500">
                      <Zap className="h-3.5 w-3.5" /> Daily registers
                    </div>
                  </div>
                </div>
              </div>
            </CardContent>
          </Card>
          <input ref={fileRef} type="file" accept=".xlsx,.xls,.csv" className="hidden" onChange={handleFileChange} />
        </motion.div>
      )}


      {/* Processing State */}
      {mode === "processing" && (
        <motion.div variants={itemVariants}>
          <Card className="p-8">
            <div className="flex flex-col items-center gap-4">
              <div className="relative">
                <div className="h-12 w-12 rounded-full border-4 border-zinc-200 dark:border-zinc-700" />
                <div className="absolute inset-0 h-12 w-12 rounded-full border-4 border-t-zinc-900 dark:border-t-zinc-100 animate-spin" />
              </div>
              <div className="text-center">
                <p className="text-sm font-semibold text-zinc-900 dark:text-zinc-100">Processing {fileName}...</p>
                <p className="text-xs text-zinc-500 mt-1">
                  {detectedFormat === "smart" && "Smart daily register format detected"}
                  {detectedFormat === "inventory" && "Inventory sheet format detected"}
                  {detectedFormat === "transactions" && "Transaction import in progress"}
                  {detectedFormat === "unknown" && "Detecting format..."}
                </p>
              </div>
            </div>
          </Card>
        </motion.div>
      )}


      {/* Results */}
      {mode === "done" && result && (
        <motion.div variants={itemVariants} className="space-y-4">
          {/* Success Card */}
          <Card className="p-6">
            <div className="flex items-start gap-4">
              <div className="flex h-11 w-11 items-center justify-center rounded-xl bg-emerald-100 dark:bg-emerald-900/30">
                <CheckCircle className="h-6 w-6 text-emerald-600 dark:text-emerald-400" />
              </div>
              <div className="flex-1">
                <p className="text-base font-semibold text-zinc-900 dark:text-zinc-50">Import Complete</p>
                <p className="text-sm text-zinc-600 dark:text-zinc-300 mt-0.5">
                  {fileName} — {detectedFormat === "smart" ? "Daily Register" : detectedFormat === "inventory" ? "Inventory Sheet" : "Transactions"}
                </p>
              </div>
              <Button variant="outline" size="sm" onClick={reset}>
                <RotateCcw className="mr-1.5 h-3.5 w-3.5" /> Import Another
              </Button>
            </div>

            {/* Stats Grid */}
            <div className="mt-5 grid grid-cols-2 sm:grid-cols-4 gap-3">
              {result.imported !== undefined && result.imported > 0 && (
                <div className="rounded-lg bg-emerald-50 dark:bg-emerald-900/10 p-3 text-center">
                  <p className="text-xl font-bold tabular-nums text-emerald-700 dark:text-emerald-400">{result.imported}</p>
                  <p className="text-[11px] font-medium text-emerald-600 dark:text-emerald-300 mt-0.5">Imported</p>
                </div>
              )}
              {result.updated !== undefined && result.updated > 0 && (
                <div className="rounded-lg bg-blue-50 dark:bg-blue-900/10 p-3 text-center">
                  <p className="text-xl font-bold tabular-nums text-blue-700 dark:text-blue-400">{result.updated}</p>
                  <p className="text-[11px] font-medium text-blue-600 dark:text-blue-300 mt-0.5">Updated</p>
                </div>
              )}
              {result.skipped !== undefined && result.skipped > 0 && (
                <div className="rounded-lg bg-amber-50 dark:bg-amber-900/10 p-3 text-center">
                  <p className="text-xl font-bold tabular-nums text-amber-700 dark:text-amber-400">{result.skipped}</p>
                  <p className="text-[11px] font-medium text-amber-600 dark:text-amber-300 mt-0.5">Skipped</p>
                </div>
              )}
              {result.partiesCreated !== undefined && result.partiesCreated > 0 && (
                <div className="rounded-lg bg-violet-50 dark:bg-violet-900/10 p-3 text-center">
                  <p className="text-xl font-bold tabular-nums text-violet-700 dark:text-violet-400">{result.partiesCreated}</p>
                  <p className="text-[11px] font-medium text-violet-600 dark:text-violet-300 mt-0.5">New Parties</p>
                </div>
              )}
              {result.typosCorrected !== undefined && result.typosCorrected > 0 && (
                <div className="rounded-lg bg-pink-50 dark:bg-pink-900/10 p-3 text-center">
                  <p className="text-xl font-bold tabular-nums text-pink-700 dark:text-pink-400">{result.typosCorrected}</p>
                  <p className="text-[11px] font-medium text-pink-600 dark:text-pink-300 mt-0.5">Typos Fixed</p>
                </div>
              )}
              {result.itemsCreated !== undefined && result.itemsCreated > 0 && (
                <div className="rounded-lg bg-cyan-50 dark:bg-cyan-900/10 p-3 text-center">
                  <p className="text-xl font-bold tabular-nums text-cyan-700 dark:text-cyan-400">{result.itemsCreated}</p>
                  <p className="text-[11px] font-medium text-cyan-600 dark:text-cyan-300 mt-0.5">New Products</p>
                </div>
              )}
              {result.quantitiesSaved !== undefined && result.quantitiesSaved > 0 && (
                <div className="rounded-lg bg-teal-50 dark:bg-teal-900/10 p-3 text-center">
                  <p className="text-xl font-bold tabular-nums text-teal-700 dark:text-teal-400">{result.quantitiesSaved}</p>
                  <p className="text-[11px] font-medium text-teal-600 dark:text-teal-300 mt-0.5">Quantities</p>
                </div>
              )}
            </div>


            {/* Smart Import Stats */}
            {result.stats && (
              <div className="mt-4 rounded-lg bg-zinc-50 dark:bg-zinc-800/50 p-4">
                <p className="text-xs font-semibold uppercase tracking-wider text-zinc-600 dark:text-zinc-300 mb-2">Breakdown</p>
                <div className="grid grid-cols-2 sm:grid-cols-4 gap-2 text-xs">
                  {result.stats.receipts > 0 && <div><span className="text-zinc-500">Receipts:</span> <span className="font-semibold">{result.stats.receipts}</span></div>}
                  {result.stats.cashSales > 0 && <div><span className="text-zinc-500">Cash Sales:</span> <span className="font-semibold">{result.stats.cashSales}</span></div>}
                  {result.stats.creditSales > 0 && <div><span className="text-zinc-500">Credit Sales:</span> <span className="font-semibold">{result.stats.creditSales}</span></div>}
                  {result.stats.upiSales > 0 && <div><span className="text-zinc-500">UPI Sales:</span> <span className="font-semibold">{result.stats.upiSales}</span></div>}
                </div>
              </div>
            )}
          </Card>

          {/* Errors */}
          {result.errors && result.errors.length > 0 && (
            <Card className="p-4">
              <div className="flex items-center gap-2 mb-3">
                <AlertTriangle className="h-4 w-4 text-amber-500" />
                <p className="text-sm font-semibold text-zinc-900 dark:text-zinc-100">
                  {result.errors.length} rows had issues
                </p>
              </div>
              <div className="max-h-48 overflow-auto rounded-lg bg-red-50/50 dark:bg-red-900/5 p-3 space-y-1">
                {result.errors.slice(0, 50).map((err, i) => (
                  <p key={i} className="text-[11px] text-red-700 dark:text-red-400 font-mono">
                    Line {err.row}: {err.reason}
                  </p>
                ))}
                {result.errors.length > 50 && (
                  <p className="text-[11px] text-red-500 mt-1">...and {result.errors.length - 50} more</p>
                )}
              </div>
            </Card>
          )}
        </motion.div>
      )}

      {/* Info Section */}
      <motion.div variants={itemVariants}>
        <Card className="p-4">
          <p className="text-xs font-semibold uppercase tracking-wider text-zinc-600 dark:text-zinc-300 mb-3">
            Supported Formats
          </p>
          <div className="grid gap-3 sm:grid-cols-3">
            <div className="rounded-lg bg-zinc-50 dark:bg-zinc-800/50 p-3">
              <div className="flex items-center gap-2 mb-1.5">
                <FileText className="h-4 w-4 text-zinc-500" />
                <p className="text-sm font-medium text-zinc-800 dark:text-zinc-200">Transactions</p>
              </div>
              <p className="text-[11px] text-zinc-500 dark:text-zinc-400">
                Columns: Date, Bill No, Party, Type, Mode, Amount. Typos auto-corrected.
              </p>
            </div>
            <div className="rounded-lg bg-zinc-50 dark:bg-zinc-800/50 p-3">
              <div className="flex items-center gap-2 mb-1.5">
                <Package className="h-4 w-4 text-zinc-500" />
                <p className="text-sm font-medium text-zinc-800 dark:text-zinc-200">Inventory Sheet</p>
              </div>
              <p className="text-[11px] text-zinc-500 dark:text-zinc-400">
                Product names + day columns (1-31). Supports qty/purchase format.
              </p>
            </div>
            <div className="rounded-lg bg-zinc-50 dark:bg-zinc-800/50 p-3">
              <div className="flex items-center gap-2 mb-1.5">
                <Zap className="h-4 w-4 text-zinc-500" />
                <p className="text-sm font-medium text-zinc-800 dark:text-zinc-200">Daily Register</p>
              </div>
              <p className="text-[11px] text-zinc-500 dark:text-zinc-400">
                COL = Receipt, SL:L = Sale. Auto-splits cash/credit/UPI.
              </p>
            </div>
          </div>
        </Card>
      </motion.div>
    </motion.div>
  );
}
