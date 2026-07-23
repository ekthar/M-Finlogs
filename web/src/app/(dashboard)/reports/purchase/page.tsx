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
import { Download } from "lucide-react";
import { springs } from "@/lib/design-tokens";

function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }
const itemVariants = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: springs.default } };

export default function PurchaseReportPage() {
  const { companyId, financialYear } = useApp();
  const [monthly, setMonthly] = useState<{month:string;total:number}[]>([]);
  const [partyWise, setPartyWise] = useState<{name:string;total:number}[]>([]);
  const [grandTotal, setGrandTotal] = useState(0);
  const [start, setStart] = useState("");
  const [end, setEnd] = useState("");
  const [loading, setLoading] = useState(false);

  const load = async () => {
    setLoading(true);
    const params = new URLSearchParams({ companyId, financialYear });
    if (start) params.set("start", start); if (end) params.set("end", end);
    try {
      const res = await fetch(`/api/reports/purchase?${params}`);
      const data = await res.json();
      setMonthly(data.monthly||[]); setPartyWise(data.partyWise||[]); setGrandTotal(data.grandTotal||0);
    } catch { toast.error("Failed"); }
    setLoading(false);
  };

  // Auto-load on mount
  useEffect(() => { load(); }, [companyId, financialYear]);

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div variants={itemVariants} className="flex items-center justify-between">
        <div><h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-100">Purchase Report</h1><p className="mt-0.5 text-sm text-zinc-500">Monthly & party-wise • Total: ₹{fmt(grandTotal)}</p></div>
        <Button variant="outline" size="sm" onClick={() => { if(partyWise.length) { exportToExcel(partyWise.map(p=>({Party:p.name,Total:p.total})),"Purchase_Report"); toast.success("Exported"); }}}><Download className="mr-1.5 h-3.5 w-3.5"/>Export</Button>
      </motion.div>

      <motion.div variants={itemVariants}><Card className="p-3"><div className="flex flex-wrap items-end gap-3">
        <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">From</label><Input type="date" value={start} onChange={e=>setStart(e.target.value)} className="w-40"/></div>
        <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">To</label><Input type="date" value={end} onChange={e=>setEnd(e.target.value)} className="w-40"/></div>
        <Button variant="secondary" size="sm" onClick={()=>{const e=new Date();setStart(new Date(e.getTime()-29*86400000).toISOString().split("T")[0]);setEnd(e.toISOString().split("T")[0]);}}>Last 30</Button>
        <Button size="sm" onClick={load} disabled={loading}>{loading?"...":"Apply"}</Button>
      </div></Card></motion.div>

      {loading ? <TableSkeleton rows={5} cols={2}/> : <>
        <motion.div variants={itemVariants}><h3 className="text-sm font-semibold text-zinc-700 dark:text-zinc-300 mb-2">Monthly</h3><Table><TableHeader><TableRow><TableHead>Month</TableHead><TableHead className="text-right">Total</TableHead></TableRow></TableHeader><TableBody>{monthly.length===0?<TableRow><TableCell colSpan={2} className="h-20 text-center text-sm text-zinc-500">No data</TableCell></TableRow>:monthly.map(m=><TableRow key={m.month}><TableCell>{m.month}</TableCell><TableCell className="text-right font-mono font-medium">₹{fmt(m.total)}</TableCell></TableRow>)}</TableBody></Table></motion.div>
        <motion.div variants={itemVariants}><h3 className="text-sm font-semibold text-zinc-700 dark:text-zinc-300 mb-2">Party-wise</h3><Table><TableHeader><TableRow><TableHead>Party</TableHead><TableHead className="text-right">Total</TableHead></TableRow></TableHeader><TableBody>{partyWise.length===0?<TableRow><TableCell colSpan={2} className="h-20 text-center text-sm text-zinc-500">No data</TableCell></TableRow>:partyWise.map(p=><TableRow key={p.name}><TableCell className="font-medium">{p.name}</TableCell><TableCell className="text-right font-mono font-medium">₹{fmt(p.total)}</TableCell></TableRow>)}</TableBody></Table></motion.div>
      </>}
    </motion.div>
  );
}
