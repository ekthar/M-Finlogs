"use client";

import { useState, useEffect } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { exportToExcel } from "@/lib/export-excel";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Badge } from "@/components/ui/badge";
import { TableSkeleton } from "@/components/ui/skeleton";
import { EditTransactionModal } from "@/components/edit-transaction-modal";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { Download, Printer, Pencil } from "lucide-react";
import { springs } from "@/lib/design-tokens";

interface ExpenseEntry { txnId:number; date:string; party:string; paymentMode:string; billNo:string|null; amount:number; }
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }
const itemVariants = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: springs.default } };

export default function ExpensesPage() {
  const { companyId, financialYear } = useApp();
  const [expenses, setExpenses] = useState<ExpenseEntry[]>([]);
  const [total, setTotal] = useState(0);
  const [start, setStart] = useState("");
  const [end, setEnd] = useState("");
  const [loading, setLoading] = useState(false);
  const [editTxn, setEditTxn] = useState<any>(null);

  const load = async () => {
    setLoading(true);
    const params = new URLSearchParams({ companyId, financialYear });
    if (start) params.set("start", start); if (end) params.set("end", end);
    try {
      const res = await fetch(`/api/reports/expenses?${params}`);
      const data = await res.json();
      setExpenses(data.expenses||[]); setTotal(data.total||0);
    } catch { toast.error("Failed"); }
    setLoading(false);
  };

  // Auto-load last 30 days on mount
  useEffect(() => {
    const e = new Date();
    const s = new Date(e.getTime() - 29 * 86400000);
    setStart(s.toISOString().split("T")[0]);
    setEnd(e.toISOString().split("T")[0]);
  }, []);

  useEffect(() => {
    if (start && end) load();
  }, [start, end, companyId, financialYear]);

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div variants={itemVariants} className="flex items-center justify-between">
        <div><h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-100">Expense Report</h1><p className="mt-0.5 text-sm text-zinc-500">{expenses.length} entries • Total: ₹{fmt(total)}</p></div>
        <div className="flex gap-2"><Button variant="outline" size="sm" onClick={()=>{if(expenses.length){exportToExcel(expenses.map(e=>({Date:e.date,Party:e.party,Mode:e.paymentMode,Bill:e.billNo||"",Amount:e.amount})),"Expenses");toast.success("Exported");}}}><Download className="mr-1.5 h-3.5 w-3.5"/>Export</Button><Button variant="outline" size="sm" onClick={()=>window.print()}><Printer className="mr-1.5 h-3.5 w-3.5"/>Print</Button></div>
      </motion.div>

      <motion.div variants={itemVariants}><Card className="p-3"><div className="flex flex-wrap items-end gap-3">
        <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">From</label><Input type="date" value={start} onChange={e=>setStart(e.target.value)} className="w-40"/></div>
        <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">To</label><Input type="date" value={end} onChange={e=>setEnd(e.target.value)} className="w-40"/></div>
        <Button variant="secondary" size="sm" onClick={()=>{const e=new Date();setStart(new Date(e.getTime()-29*86400000).toISOString().split("T")[0]);setEnd(e.toISOString().split("T")[0]);}}>Last 30</Button>
        <Button size="sm" onClick={load} disabled={loading}>{loading?"...":"Show"}</Button>
      </div></Card></motion.div>

      <motion.div variants={itemVariants}>{loading ? <TableSkeleton rows={8} cols={5}/> : <Table><TableHeader><TableRow><TableHead>Date</TableHead><TableHead>Party</TableHead><TableHead>Mode</TableHead><TableHead>Bill</TableHead><TableHead className="text-right">Amount</TableHead><TableHead className="w-12"></TableHead></TableRow></TableHeader><TableBody>
        {expenses.length===0 ? <TableRow><TableCell colSpan={6} className="h-32 text-center text-sm text-zinc-500">Select dates and click Show</TableCell></TableRow> : expenses.map(e=><TableRow key={e.txnId}><TableCell className="text-xs">{new Date(e.date).toLocaleDateString("en-IN")}</TableCell><TableCell className="font-medium text-xs">{e.party}</TableCell><TableCell><Badge>{e.paymentMode}</Badge></TableCell><TableCell className="text-xs">{e.billNo||"—"}</TableCell><TableCell className="text-right font-mono text-xs font-medium">₹{fmt(e.amount)}</TableCell><TableCell><Button variant="ghost" size="icon" className="h-7 w-7" onClick={()=>setEditTxn({txnId:e.txnId,txnDate:e.date,billNo:e.billNo,txnType:"Expense",paymentMode:e.paymentMode,amount:String(e.amount),party:{name:e.party}})}><Pencil className="h-3 w-3"/></Button></TableCell></TableRow>)}
      </TableBody></Table>}</motion.div>

      <EditTransactionModal transaction={editTxn} open={!!editTxn} onClose={()=>setEditTxn(null)} onSaved={load}/>
    </motion.div>
  );
}
