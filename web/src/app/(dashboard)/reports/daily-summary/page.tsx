"use client";

import { useState } from "react";
import { motion } from "framer-motion";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { Download, Printer } from "lucide-react";

interface SummaryRow { date: string; openingCash: number; cashIn: number; cashExp: number; cashNeeded: number; cashInHand: number; shortExcess: number; bank: number; credit: number; totalSales: number; }
const fadeUp = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }

export default function DailySummaryPage() {
  const [start, setStart] = useState("");
  const [end, setEnd] = useState("");
  const [rows, setRows] = useState<SummaryRow[]>([]);
  const [loading, setLoading] = useState(false);

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

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div><h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Daily Summary</h1><p className="mt-0.5 text-sm text-zinc-500">Cash flow with opening/closing balances</p></div>
        <Button variant="outline" size="sm" onClick={() => window.print()}><Printer className="mr-1.5 h-3.5 w-3.5" /> Print</Button>
      </motion.div>

      <motion.div {...fadeUp}><Card className="p-4"><div className="flex flex-wrap items-end gap-3">
        <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">From</label><Input type="date" value={start} onChange={(e) => setStart(e.target.value)} className="w-40" /></div>
        <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">To</label><Input type="date" value={end} onChange={(e) => setEnd(e.target.value)} className="w-40" /></div>
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
              <TableRow><TableCell colSpan={10} className="h-32 text-center"><p className="text-sm text-zinc-500">Select a date range</p></TableCell></TableRow>
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
