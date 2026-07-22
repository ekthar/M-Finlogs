"use client";

import { useState } from "react";
import { motion } from "framer-motion";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { Download, Printer, FileText } from "lucide-react";

interface SummaryRow { date: string; openingCash: number; cashIn: number; cashExp: number; cashNeeded: number; cashInHand: number; shortExcess: number; bank: number; credit: number; totalSales: number; }
const fadeUp = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }

export default function DailySummaryPage() {
  const [start, setStart] = useState("");
  const [end, setEnd] = useState("");
  const [rows, setRows] = useState<SummaryRow[]>([]);
  const [loading, setLoading] = useState(false);
  const [pdfLoading, setPdfLoading] = useState(false);
  const [excelLoading, setExcelLoading] = useState(false);

  const load = async (days?: number) => {
    setLoading(true);
    const params = new URLSearchParams();
    if (days) {
      const e = new Date(); const s = new Date(e.getTime() - (days - 1) * 86400000);
      params.set("start", s.toISOString().split("T")[0]);
      params.set("end", e.toISOString().split("T")[0]);
    } else {
      if (start) params.set("start", start);
      if (end) params.set("end", end);
    }
    try {
      const res = await fetch(`/api/reports/daily-summary?${params}`);
      const data = await res.json();
      setRows(data.summary || []);
    } catch { /* */ }
    setLoading(false);
  };

  const handlePdfExport = async () => {
    if (rows.length === 0) return;
    setPdfLoading(true);
    try {
      const { exportDailySummaryPdf } = await import("@/lib/export-pdf");
      await exportDailySummaryPdf({ rows });
    } catch { /* */ }
    setPdfLoading(false);
  };

  const handleExcelExport = async () => {
    if (rows.length === 0) return;
    setExcelLoading(true);
    try {
      const { exportTableExcel } = await import("@/lib/export-excel");
      await exportTableExcel({
        sheetName: "Daily Summary",
        headers: ["Date", "Opening", "Cash In", "Cash Exp", "Needed", "In Hand", "Short/Excess", "Bank", "Credit", "Total Sales"],
        rows: rows.map((r) => [new Date(r.date).toLocaleDateString("en-IN"), r.openingCash, r.cashIn, r.cashExp, r.cashNeeded, r.cashInHand, r.shortExcess, r.bank, r.credit, r.totalSales]),
        filename: "daily_summary.xlsx",
      });
    } catch { /* */ }
    setExcelLoading(false);
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div><h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-50">Daily Summary</h1><p className="mt-1 text-sm text-zinc-600 dark:text-zinc-300">Cash flow with opening/closing balances</p></div>
        <div className="flex items-center gap-2">
          <Button variant="outline" size="sm" onClick={handlePdfExport} disabled={pdfLoading || rows.length === 0}>
            <FileText className="mr-1.5 h-3.5 w-3.5" /> {pdfLoading ? "..." : "PDF"}
          </Button>
          <Button variant="outline" size="sm" onClick={handleExcelExport} disabled={excelLoading || rows.length === 0}>
            <Download className="mr-1.5 h-3.5 w-3.5" /> {excelLoading ? "..." : "Excel"}
          </Button>
          <Button variant="outline" size="sm" onClick={() => window.print()}>
            <Printer className="mr-1.5 h-3.5 w-3.5" /> Print
          </Button>
        </div>
      </motion.div>

      <motion.div {...fadeUp}><Card className="p-4"><div className="flex flex-wrap items-end gap-3">
        <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">From</label><Input type="date" value={start} onChange={(e) => setStart(e.target.value)} className="w-40" /></div>
        <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">To</label><Input type="date" value={end} onChange={(e) => setEnd(e.target.value)} className="w-40" /></div>
        <Button variant="secondary" size="sm" onClick={() => load(30)}>Last 30 Days</Button>
        <Button variant="secondary" size="sm" onClick={() => load(90)}>Last 90 Days</Button>
        <Button size="sm" onClick={() => load()} disabled={loading}>{loading ? "..." : "Apply"}</Button>
      </div></Card></motion.div>

      <motion.div {...fadeUp}>
        <Table>
          <TableHeader><TableRow>
            <TableHead>Date</TableHead><TableHead className="text-right">Opening</TableHead><TableHead className="text-right">Cash In</TableHead><TableHead className="text-right">Cash Exp</TableHead><TableHead className="text-right">Needed</TableHead><TableHead className="text-right">In Hand</TableHead><TableHead className="text-right">Short/Excess</TableHead><TableHead className="text-right">Bank</TableHead><TableHead className="text-right">Credit</TableHead><TableHead className="text-right">Total Sales</TableHead>
          </TableRow></TableHeader>
          <TableBody>
            {rows.length === 0 ? (
              <TableRow><TableCell colSpan={10} className="h-32 text-center"><p className="text-sm text-zinc-600 dark:text-zinc-300">Select a date range</p></TableCell></TableRow>
            ) : rows.map((r) => (
              <TableRow key={r.date}>
                <TableCell>{new Date(r.date).toLocaleDateString("en-IN")}</TableCell>
                <TableCell className="text-right font-mono">{fmt(r.openingCash)}</TableCell>
                <TableCell className="text-right font-mono">{fmt(r.cashIn)}</TableCell>
                <TableCell className="text-right font-mono">{fmt(r.cashExp)}</TableCell>
                <TableCell className="text-right font-mono">{fmt(r.cashNeeded)}</TableCell>
                <TableCell className="text-right font-mono">{fmt(r.cashInHand)}</TableCell>
                <TableCell className={`text-right font-mono font-medium ${r.shortExcess < 0 ? "text-red-500" : "text-emerald-600"}`}>{fmt(r.shortExcess)}</TableCell>
                <TableCell className="text-right font-mono">{fmt(r.bank)}</TableCell>
                <TableCell className="text-right font-mono">{fmt(r.credit)}</TableCell>
                <TableCell className="text-right font-mono font-medium">{fmt(r.totalSales)}</TableCell>
              </TableRow>
            ))}
          </TableBody>
        </Table>
      </motion.div>
    </motion.div>
  );
}
