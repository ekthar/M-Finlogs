"use client";

import { useState } from "react";
import { motion } from "framer-motion";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { Badge } from "@/components/ui/badge";
import { Printer } from "lucide-react";

interface DayBookEntry { txnId: number; date: string; billNo: string | null; party: string; txnType: string; paymentMode: string; amount: number; }
const fadeUp = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }

export default function DayBookPage() {
  const [date, setDate] = useState(new Date().toISOString().split("T")[0]);
  const [entries, setEntries] = useState<DayBookEntry[]>([]);
  const [total, setTotal] = useState(0);
  const [loading, setLoading] = useState(false);

  const load = async () => {
    setLoading(true);
    try {
      const res = await fetch(`/api/reports/day-book?date=${date}`);
      const data = await res.json();
      setEntries(data.transactions || []);
      setTotal(data.total || 0);
    } catch { /* */ }
    setLoading(false);
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Day Book</h1>
          <p className="mt-0.5 text-sm text-zinc-500">All transactions for a specific date • Total: ₹{fmt(total)}</p>
        </div>
        <Button variant="outline" size="sm" onClick={() => window.print()}><Printer className="mr-1.5 h-3.5 w-3.5" /> Print</Button>
      </motion.div>

      <motion.div {...fadeUp}>
        <Card className="p-4"><div className="flex items-end gap-4">
          <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Date</label><Input type="date" value={date} onChange={(e) => setDate(e.target.value)} className="w-44" /></div>
          <Button size="sm" onClick={load} disabled={loading}>{loading ? "Loading..." : "Show"}</Button>
        </div></Card>
      </motion.div>

      <motion.div {...fadeUp}>
        <Table>
          <TableHeader><TableRow><TableHead>Date</TableHead><TableHead>Bill</TableHead><TableHead>Party</TableHead><TableHead>Type</TableHead><TableHead>Mode</TableHead><TableHead className="text-right">Amount</TableHead></TableRow></TableHeader>
          <TableBody>
            {entries.length === 0 ? (
              <TableRow><TableCell colSpan={6} className="h-32 text-center"><p className="text-sm text-zinc-500">{loading ? "Loading..." : "Select a date and click Show"}</p></TableCell></TableRow>
            ) : entries.map((e) => (
              <TableRow key={e.txnId}>
                <TableCell>{new Date(e.date).toLocaleDateString("en-IN")}</TableCell>
                <TableCell>{e.billNo || "—"}</TableCell>
                <TableCell className="font-medium">{e.party}</TableCell>
                <TableCell><Badge variant={e.txnType === "Sale" ? "success" : e.txnType === "Expense" ? "danger" : "default"}>{e.txnType}</Badge></TableCell>
                <TableCell><Badge variant="info">{e.paymentMode}</Badge></TableCell>
                <TableCell className="text-right font-mono">₹{fmt(e.amount)}</TableCell>
              </TableRow>
            ))}
          </TableBody>
        </Table>
      </motion.div>
    </motion.div>
  );
}
