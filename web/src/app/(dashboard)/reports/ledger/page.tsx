"use client";

import { useState } from "react";
import { motion } from "framer-motion";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { Download, Printer, Search } from "lucide-react";

interface LedgerEntry { date: string; billNo: string | null; txnType: string; paymentMode: string; debit: number; credit: number; balance: number; }
const fadeUp = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }

export default function LedgerPage() {
  const [party, setParty] = useState("");
  const [start, setStart] = useState("");
  const [end, setEnd] = useState("");
  const [ledger, setLedger] = useState<LedgerEntry[]>([]);
  const [loading, setLoading] = useState(false);
  const [partyName, setPartyName] = useState("");

  const load = async () => {
    if (!party) return;
    setLoading(true);
    try {
      const params = new URLSearchParams({ party });
      if (start) params.set("start", start);
      if (end) params.set("end", end);
      const res = await fetch(`/api/reports/ledger?${params}`);
      const data = await res.json();
      if (res.ok) { setLedger(data.ledger || []); setPartyName(data.party || party); }
      else alert(data.error || "Failed");
    } catch { alert("Network error"); }
    setLoading(false);
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Party Ledger</h1>
          <p className="mt-0.5 text-sm text-zinc-500">{partyName ? `Showing: ${partyName}` : "Account-wise debit/credit statement"}</p>
        </div>
        <div className="flex gap-2">
          <Button variant="outline" size="sm" onClick={() => window.print()}><Printer className="mr-1.5 h-3.5 w-3.5" /> Print</Button>
        </div>
      </motion.div>

      <motion.div {...fadeUp}>
        <Card className="p-4">
          <div className="flex flex-wrap items-end gap-4">
            <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Party</label><Input placeholder="Enter party name..." value={party} onChange={(e) => setParty(e.target.value)} className="w-52" /></div>
            <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">From</label><Input type="date" value={start} onChange={(e) => setStart(e.target.value)} className="w-40" /></div>
            <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">To</label><Input type="date" value={end} onChange={(e) => setEnd(e.target.value)} className="w-40" /></div>
            <Button size="sm" onClick={load} disabled={loading}>{loading ? "Loading..." : "Show"}</Button>
          </div>
        </Card>
      </motion.div>

      <motion.div {...fadeUp}>
        <Table>
          <TableHeader><TableRow><TableHead>Date</TableHead><TableHead>Bill</TableHead><TableHead>Type</TableHead><TableHead>Mode</TableHead><TableHead className="text-right">Debit</TableHead><TableHead className="text-right">Credit</TableHead><TableHead className="text-right">Balance</TableHead></TableRow></TableHeader>
          <TableBody>
            {ledger.length === 0 ? (
              <TableRow><TableCell colSpan={7} className="h-32 text-center"><p className="text-sm text-zinc-500">Select a party to view ledger</p></TableCell></TableRow>
            ) : ledger.map((e, i) => (
              <TableRow key={i}>
                <TableCell>{new Date(e.date).toLocaleDateString("en-IN")}</TableCell>
                <TableCell>{e.billNo || "—"}</TableCell>
                <TableCell>{e.txnType}</TableCell>
                <TableCell>{e.paymentMode}</TableCell>
                <TableCell className="text-right font-mono">{e.debit > 0 ? fmt(e.debit) : ""}</TableCell>
                <TableCell className="text-right font-mono">{e.credit > 0 ? fmt(e.credit) : ""}</TableCell>
                <TableCell className="text-right font-mono font-medium">{fmt(e.balance)}</TableCell>
              </TableRow>
            ))}
          </TableBody>
        </Table>
      </motion.div>
    </motion.div>
  );
}
