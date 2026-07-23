"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { exportOutstandingPdf } from "@/lib/export-pdf";
import { exportToExcel } from "@/lib/export-excel";
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

  const positiveBalance = data.filter(e => e.balance > 0);
  const negativeBalance = data.filter(e => e.balance < 0);
  const totalPositive = positiveBalance.reduce((s, e) => s + e.balance, 0);
  const totalNegative = negativeBalance.reduce((s, e) => s + e.balance, 0);

  const handleExcelExport = () => {
    if (!data.length) return;
    const rows = data.map((e, i) => ({
      "Sr No": i + 1,
      "Party Name": e.name,
      "Status": e.status,
      "Days Unpaid": e.daysUnpaid,
      "Last Receipt": e.lastReceipt || "Never",
      "Outstanding (₹)": e.balance,
      "Type": e.balance >= 0 ? "Receivable" : "Overpaid",
    }));
    // Add summary rows
    rows.push({ "Sr No": 0, "Party Name": "", "Status": "", "Days Unpaid": 0, "Last Receipt": "", "Outstanding (₹)": 0, "Type": "" } as any);
    rows.push({ "Sr No": 0, "Party Name": "TOTAL RECEIVABLE", "Status": "", "Days Unpaid": 0, "Last Receipt": "", "Outstanding (₹)": totalPositive, "Type": "" } as any);
    rows.push({ "Sr No": 0, "Party Name": "TOTAL OVERPAID", "Status": "", "Days Unpaid": 0, "Last Receipt": "", "Outstanding (₹)": totalNegative, "Type": "" } as any);
    rows.push({ "Sr No": 0, "Party Name": "NET OUTSTANDING", "Status": "", "Days Unpaid": 0, "Last Receipt": "", "Outstanding (₹)": totalPositive + totalNegative, "Type": "" } as any);

    exportToExcel(rows, `Outstanding_Report_${new Date().toISOString().split("T")[0]}`, "Outstanding");
    toast.success("Excel exported with full details");
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-50">Credit Outstanding</h1>
          <p className="mt-1 text-sm text-zinc-600 dark:text-zinc-300">
            {loading ? "Loading..." : `${data.length} credit parties • excludes generic "Customer"`}
          </p>
        </div>
        <div className="flex gap-2">
          <Button variant="outline" size="sm" onClick={handleExcelExport} disabled={!data.length}>
            <Download className="mr-1.5 h-3.5 w-3.5" />Excel
          </Button>
          <Button variant="outline" size="sm" onClick={() => { if (data.length) { exportOutstandingPdf(data, total); toast.success("PDF exported"); } }}>
            <FileText className="mr-1.5 h-3.5 w-3.5" />PDF
          </Button>
        </div>
      </motion.div>

      <motion.div {...fadeUp} className="grid grid-cols-2 gap-3 sm:grid-cols-5">
        <Card className="p-4 text-center">
          <p className="text-xs font-medium text-zinc-600 dark:text-zinc-300">Total Receivable</p>
          <p className="mt-1 text-xl font-bold tabular-nums text-red-600">₹{fmt(totalPositive)}</p>
        </Card>
        <Card className="p-4 text-center">
          <p className="text-xs font-medium text-zinc-600 dark:text-zinc-300">Overpaid</p>
          <p className="mt-1 text-xl font-bold tabular-nums text-emerald-600">₹{fmt(Math.abs(totalNegative))}</p>
        </Card>
        <Card className="p-4 text-center">
          <p className="text-xs font-medium text-zinc-600 dark:text-zinc-300">15+ Days Due</p>
          <p className="mt-1 text-xl font-bold tabular-nums text-amber-500">{highCount}</p>
        </Card>
        <Card className="p-4 text-center">
          <p className="text-xs font-medium text-zinc-600 dark:text-zinc-300">30+ Critical</p>
          <p className="mt-1 text-xl font-bold tabular-nums text-red-600">{criticalCount}</p>
        </Card>
        <Card className="p-4 text-center">
          <p className="text-xs font-medium text-zinc-600 dark:text-zinc-300">Parties</p>
          <p className="mt-1 text-xl font-bold tabular-nums text-zinc-900 dark:text-zinc-50">{data.length}</p>
        </Card>
      </motion.div>

      {/* Receivable (positive balance) */}
      <motion.div {...fadeUp}>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead className="w-10">#</TableHead>
              <TableHead>Party</TableHead>
              <TableHead>Status</TableHead>
              <TableHead className="text-right">Days</TableHead>
              <TableHead>Last Receipt</TableHead>
              <TableHead className="text-right">Outstanding</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            {data.length === 0 ? (
              <TableRow><TableCell colSpan={6} className="h-32 text-center"><p className="text-sm text-zinc-600 dark:text-zinc-300">{loading ? "Loading..." : "No credit outstanding"}</p></TableCell></TableRow>
            ) : data.map((e, i) => (
              <TableRow key={e.partyId}>
                <TableCell className="text-xs text-zinc-500 tabular-nums">{i + 1}</TableCell>
                <TableCell className="font-medium">{e.name}</TableCell>
                <TableCell>
                  <Badge variant={e.status === "Critical" ? "danger" : e.status === "High" ? "warning" : e.balance < 0 ? "info" : "success"}>
                    {e.balance < 0 ? "Overpaid" : e.status}
                  </Badge>
                </TableCell>
                <TableCell className="text-right tabular-nums">{e.daysUnpaid}</TableCell>
                <TableCell className="text-sm">{e.lastReceipt || "Never"}</TableCell>
                <TableCell className={`text-right font-mono font-semibold tabular-nums ${e.balance >= 0 ? "text-red-600" : "text-emerald-600"}`}>
                  {e.balance >= 0 ? `₹${fmt(e.balance)}` : `-₹${fmt(Math.abs(e.balance))}`}
                </TableCell>
              </TableRow>
            ))}
            {data.length > 0 && (
              <TableRow className="border-t-2 border-zinc-300 dark:border-zinc-600 font-bold">
                <TableCell colSpan={5} className="text-sm font-semibold">Net Outstanding</TableCell>
                <TableCell className="text-right font-mono font-bold tabular-nums text-zinc-900 dark:text-zinc-100">
                  ₹{fmt(totalPositive + totalNegative)}
                </TableCell>
              </TableRow>
            )}
          </TableBody>
        </Table>
      </motion.div>
    </motion.div>
  );
}
