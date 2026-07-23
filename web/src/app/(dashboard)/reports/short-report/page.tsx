"use client";

import { useState, useEffect } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { exportToExcel } from "@/lib/export-excel";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { TableSkeleton } from "@/components/ui/skeleton";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { Download, Printer } from "lucide-react";
import { springs } from "@/lib/design-tokens";

interface ShortRow { date: string; openingCash: number; cashIn: number; cashExp: number; cashNeeded: number; cashInHand: number; shortExcess: number; }
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }
const itemVariants = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: springs.default } };

export default function ShortReportPage() {
  const { companyId } = useApp();
  const [rows, setRows] = useState<ShortRow[]>([]);
  const [totalShort, setTotalShort] = useState(0);
  const [totalExcess, setTotalExcess] = useState(0);
  const [start, setStart] = useState("");
  const [end, setEnd] = useState("");
  const [loading, setLoading] = useState(false);

  const load = async (days?: number) => {
    setLoading(true);
    const params = new URLSearchParams({ companyId });
    if (days) {
      const e = new Date(); const s = new Date(e.getTime() - (days-1)*86400000);
      params.set("start", s.toISOString().split("T")[0]); params.set("end", e.toISOString().split("T")[0]);
    } else { if (start) params.set("start", start); if (end) params.set("end", end); }
    try {
      const res = await fetch(`/api/reports/short-report?${params}`);
      const data = await res.json();
      setRows(data.rows || []); setTotalShort(data.totalShort || 0); setTotalExcess(data.totalExcess || 0);
    } catch { toast.error("Failed to load"); }
    setLoading(false);
  };

  // Auto-load last 30 days on mount
  useEffect(() => { load(30); }, [companyId]);

  const handleExport = () => {
    if (!rows.length) return;
    exportToExcel(rows.map(r => ({ Date: r.date, "Opening Cash": r.openingCash, "Cash In": r.cashIn, "Cash Expense": r.cashExp, "Cash Needed": r.cashNeeded, "Cash In Hand": r.cashInHand, "Short/Excess": r.shortExcess })), "Short_Excess_Report");
    toast.success("Excel exported");
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div variants={itemVariants} className="flex items-center justify-between">
        <div><h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-100">Short / Excess Report</h1><p className="mt-0.5 text-sm text-zinc-500">Track daily cash shortages</p></div>
        <div className="flex gap-2"><Button variant="outline" size="sm" onClick={handleExport}><Download className="mr-1.5 h-3.5 w-3.5"/>Export</Button><Button variant="outline" size="sm" onClick={() => window.print()}><Printer className="mr-1.5 h-3.5 w-3.5"/>Print</Button></div>
      </motion.div>

      <motion.div variants={itemVariants}><Card className="p-3"><div className="flex flex-wrap items-end gap-3">
        <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">From</label><Input type="date" value={start} onChange={e=>setStart(e.target.value)} className="w-40"/></div>
        <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">To</label><Input type="date" value={end} onChange={e=>setEnd(e.target.value)} className="w-40"/></div>
        <Button variant="secondary" size="sm" onClick={()=>load(30)}>Last 30 Days</Button>
        <Button variant="secondary" size="sm" onClick={()=>load(90)}>Last 90 Days</Button>
        <Button size="sm" onClick={()=>load()} disabled={loading}>{loading?"...":"Apply"}</Button>
      </div></Card></motion.div>

      {rows.length > 0 && <motion.div variants={itemVariants} className="flex gap-4"><Card className="p-3 flex-1 text-center"><p className="text-[11px] uppercase tracking-wider text-zinc-500">Total Short</p><p className="mt-1 text-lg font-semibold text-red-500">₹{fmt(Math.abs(totalShort))}</p></Card><Card className="p-3 flex-1 text-center"><p className="text-[11px] uppercase tracking-wider text-zinc-500">Total Excess</p><p className="mt-1 text-lg font-semibold text-emerald-600">₹{fmt(totalExcess)}</p></Card></motion.div>}

      <motion.div variants={itemVariants}>{loading ? <TableSkeleton rows={8} cols={7}/> : <Table><TableHeader><TableRow><TableHead>Date</TableHead><TableHead className="text-right">Opening</TableHead><TableHead className="text-right">Cash In</TableHead><TableHead className="text-right">Cash Exp</TableHead><TableHead className="text-right">Needed</TableHead><TableHead className="text-right">In Hand</TableHead><TableHead className="text-right">Short/Excess</TableHead></TableRow></TableHeader><TableBody>
        {rows.length===0 ? <TableRow><TableCell colSpan={7} className="h-32 text-center text-sm text-zinc-500">Select a date range</TableCell></TableRow> : rows.map(r=><TableRow key={r.date}><TableCell className="text-xs">{new Date(r.date).toLocaleDateString("en-IN")}</TableCell><TableCell className="text-right font-mono text-xs">{fmt(r.openingCash)}</TableCell><TableCell className="text-right font-mono text-xs">{fmt(r.cashIn)}</TableCell><TableCell className="text-right font-mono text-xs">{fmt(r.cashExp)}</TableCell><TableCell className="text-right font-mono text-xs">{fmt(r.cashNeeded)}</TableCell><TableCell className="text-right font-mono text-xs">{fmt(r.cashInHand)}</TableCell><TableCell className={`text-right font-mono text-xs font-medium ${r.shortExcess<0?"text-red-500":"text-emerald-600"}`}>{r.shortExcess<0?"-":"+"}{fmt(Math.abs(r.shortExcess))}</TableCell></TableRow>)}
      </TableBody></Table>}</motion.div>
    </motion.div>
  );
}
