"use client";

import { useState } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Badge } from "@/components/ui/badge";
import { TableSkeleton } from "@/components/ui/skeleton";
import { ConfirmDialog } from "@/components/ui/confirm-dialog";
import { EditTransactionModal } from "@/components/edit-transaction-modal";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { Printer, Pencil, Trash2 } from "lucide-react";

interface DayBookEntry { txnId: number; date: string; billNo: string | null; party: string; txnType: string; paymentMode: string; amount: number; }
const fadeUp = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }

export default function DayBookPage() {
  const { companyId } = useApp();
  const [date, setDate] = useState(new Date().toISOString().split("T")[0]);
  const [entries, setEntries] = useState<DayBookEntry[]>([]);
  const [total, setTotal] = useState(0);
  const [loading, setLoading] = useState(false);
  const [editTxn, setEditTxn] = useState<any>(null);
  const [deleteId, setDeleteId] = useState<number | null>(null);

  const load = async () => {
    setLoading(true);
    try {
      const res = await fetch(`/api/reports/day-book?date=${date}&companyId=${companyId}`);
      const data = await res.json();
      setEntries(data.transactions || []); setTotal(data.total || 0);
    } catch {}
    setLoading(false);
  };

  const handleDelete = async () => {
    if (deleteId === null) return;
    try { await fetch(`/api/transactions/${deleteId}`, { method: "DELETE" }); toast.success("Deleted"); load(); } catch { toast.error("Failed"); }
    setDeleteId(null);
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div><h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Day Book</h1><p className="mt-0.5 text-sm text-zinc-500">{entries.length} entries • Total: ₹{fmt(total)}</p></div>
        <Button variant="outline" size="sm" onClick={() => window.print()}><Printer className="mr-1.5 h-3.5 w-3.5" />Print</Button>
      </motion.div>

      <motion.div {...fadeUp}><Card className="p-4"><div className="flex items-end gap-4"><div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Date</label><Input type="date" value={date} onChange={e => setDate(e.target.value)} className="w-44"/></div><Button size="sm" onClick={load} disabled={loading}>{loading ? "..." : "Show"}</Button></div></Card></motion.div>

      <motion.div {...fadeUp}>
        {loading ? <TableSkeleton rows={8} cols={7}/> : <Table><TableHeader><TableRow><TableHead>Date</TableHead><TableHead>Bill</TableHead><TableHead>Party</TableHead><TableHead>Type</TableHead><TableHead>Mode</TableHead><TableHead className="text-right">Amount</TableHead><TableHead className="w-20 text-center">Edit</TableHead></TableRow></TableHeader><TableBody>
          {entries.length === 0 ? <TableRow><TableCell colSpan={7} className="h-32 text-center"><p className="text-sm text-zinc-500">Select a date and click Show</p></TableCell></TableRow> : entries.map(e => (
            <TableRow key={e.txnId}>
              <TableCell className="text-xs">{new Date(e.date).toLocaleDateString("en-IN")}</TableCell>
              <TableCell className="text-xs">{e.billNo || "—"}</TableCell>
              <TableCell className="font-medium text-xs">{e.party}</TableCell>
              <TableCell><Badge variant={e.txnType==="Sale"?"success":e.txnType==="Expense"?"danger":"info"}>{e.txnType}</Badge></TableCell>
              <TableCell><Badge>{e.paymentMode}</Badge></TableCell>
              <TableCell className="text-right font-mono text-xs font-medium">₹{fmt(e.amount)}</TableCell>
              <TableCell className="text-center"><div className="flex justify-center gap-0.5">
                <Button variant="ghost" size="icon" className="h-7 w-7" onClick={() => setEditTxn({ txnId: e.txnId, txnDate: e.date, billNo: e.billNo, txnType: e.txnType, paymentMode: e.paymentMode, amount: String(e.amount), party: { name: e.party } })}><Pencil className="h-3 w-3"/></Button>
                <Button variant="ghost" size="icon" className="h-7 w-7 text-red-500" onClick={() => setDeleteId(e.txnId)}><Trash2 className="h-3 w-3"/></Button>
              </div></TableCell>
            </TableRow>
          ))}
        </TableBody></Table>}
      </motion.div>

      <EditTransactionModal transaction={editTxn} open={!!editTxn} onClose={() => setEditTxn(null)} onSaved={load}/>
      <ConfirmDialog open={deleteId!==null} title="Delete Transaction" message="Permanently delete this transaction?" variant="danger" confirmLabel="Delete" onConfirm={handleDelete} onCancel={() => setDeleteId(null)}/>
    </motion.div>
  );
}
