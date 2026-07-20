"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { exportOutstandingPdf } from "@/lib/export-pdf";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { Download, FileText } from "lucide-react";

interface OutstandingEntry { partyId: number; name: string; balance: number; daysUnpaid: number; lastReceipt: string | null; status: string; }
const fadeUp = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }

export default function OutstandingPage() {
  const { companyId } = useApp();
  const [data, setData] = useState<OutstandingEntry[]>([]);
  const [total, setTotal] = useState(0);
  const [highCount, setHighCount] = useState(0);
  const [criticalCount, setCriticalCount] = useState(0);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    fetch(`/api/reports/outstanding?companyId=${companyId}`).then(r => r.json()).then(d => {
      setData(d.outstanding || []);
      setTotal(d.total || 0);
      setHighCount(d.highCount || 0);
      setCriticalCount(d.criticalCount || 0);
    }).catch(() => {}).finally(() => setLoading(false));
  }, [companyId]);

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div><h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Credit Outstanding</h1><p className="mt-0.5 text-sm text-zinc-500">{loading ? "Loading..." : `${data.length} parties with outstanding balance`}</p></div>
        <Button variant="outline" size="sm" onClick={() => { if (data.length) { exportOutstandingPdf(data, total); toast.success("PDF exported"); }}}><FileText className="mr-1.5 h-3.5 w-3.5"/>Export PDF</Button>
      </motion.div>

      <motion.div {...fadeUp} className="grid grid-cols-2 gap-3 sm:grid-cols-4">
        <Card className="p-4 text-center"><p className="text-xs font-medium text-zinc-500">Total Outstanding</p><p className="mt-1 text-xl font-semibold text-red-500">₹{fmt(total)}</p></Card>
        <Card className="p-4 text-center"><p className="text-xs font-medium text-zinc-500">15+ Days Due</p><p className="mt-1 text-xl font-semibold text-amber-500">{highCount}</p></Card>
        <Card className="p-4 text-center"><p className="text-xs font-medium text-zinc-500">30+ Critical</p><p className="mt-1 text-xl font-semibold text-red-600">{criticalCount}</p></Card>
        <Card className="p-4 text-center"><p className="text-xs font-medium text-zinc-500">Parties</p><p className="mt-1 text-xl font-semibold text-zinc-900 dark:text-zinc-100">{data.length}</p></Card>
      </motion.div>

      <motion.div {...fadeUp}>
        <Table>
          <TableHeader><TableRow><TableHead>Party</TableHead><TableHead>Status</TableHead><TableHead className="text-right">Days Unpaid</TableHead><TableHead>Last Receipt</TableHead><TableHead className="text-right">Outstanding</TableHead></TableRow></TableHeader>
          <TableBody>
            {data.length === 0 ? (
              <TableRow><TableCell colSpan={5} className="h-32 text-center"><p className="text-sm text-zinc-500">{loading ? "Loading..." : "No outstanding credits"}</p></TableCell></TableRow>
            ) : data.map((e) => (
              <TableRow key={e.partyId}>
                <TableCell className="font-medium">{e.name}</TableCell>
                <TableCell><Badge variant={e.status === "Critical" ? "danger" : e.status === "High" ? "warning" : "success"}>{e.status}</Badge></TableCell>
                <TableCell className="text-right">{e.daysUnpaid}</TableCell>
                <TableCell>{e.lastReceipt || "Never"}</TableCell>
                <TableCell className="text-right font-mono font-medium text-red-500">₹{fmt(e.balance)}</TableCell>
              </TableRow>
            ))}
          </TableBody>
        </Table>
      </motion.div>
    </motion.div>
  );
}
