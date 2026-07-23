"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { exportTrialBalancePdf } from "@/lib/export-pdf";
import { exportToExcel } from "@/lib/export-excel";
import { Button } from "@/components/ui/button";
import { Card } from "@/components/ui/card";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { FileText, Download, Printer } from "lucide-react";

interface Account { name: string; type: string; debit: number; credit: number; }
const fadeUp = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }

export default function TrialBalancePage() {
  const { companyId, financialYear } = useApp();
  const [accounts, setAccounts] = useState<Account[]>([]);
  const [totalDebit, setTotalDebit] = useState(0);
  const [totalCredit, setTotalCredit] = useState(0);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    fetch(`/api/reports/trial-balance?companyId=${companyId}&financialYear=${financialYear}`)
      .then(r => r.json())
      .then(d => {
        setAccounts(d.accounts || []);
        setTotalDebit(d.totalDebit || 0);
        setTotalCredit(d.totalCredit || 0);
      })
      .catch(() => {})
      .finally(() => setLoading(false));
  }, [companyId, financialYear]);

  const difference = Math.abs(totalDebit - totalCredit);

  const handlePdf = () => {
    if (accounts.length) { exportTrialBalancePdf(accounts, totalDebit, totalCredit); toast.success("PDF exported"); }
  };
  const handleExcel = () => {
    if (!accounts.length) return;
    exportToExcel(
      accounts.map((a, i) => ({ "#": i + 1, "Account / Party": a.name, Type: a.type, "Debit (₹)": a.debit || "", "Credit (₹)": a.credit || "" })),
      `Trial_Balance_FY${financialYear}`, "Trial Balance",
      { totalsRow: { "#": "", "Account / Party": "TOTAL", Type: "", "Debit (₹)": totalDebit, "Credit (₹)": totalCredit } }
    );
    toast.success("Excel exported");
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-50">Trial Balance</h1>
          <p className="mt-1 text-sm text-zinc-600 dark:text-zinc-300">
            {loading ? "Loading..." : `FY ${financialYear} • ${accounts.length} accounts`}
            {!loading && difference > 0.01 && ` • Difference: ₹${fmt(difference)}`}
          </p>
        </div>
        <div className="flex gap-2">
          <Button variant="outline" size="sm" onClick={handlePdf} disabled={!accounts.length}><FileText className="mr-1.5 h-3.5 w-3.5"/>PDF</Button>
          <Button variant="outline" size="sm" onClick={handleExcel} disabled={!accounts.length}><Download className="mr-1.5 h-3.5 w-3.5"/>Excel</Button>
          <Button variant="outline" size="sm" onClick={() => window.print()}><Printer className="mr-1.5 h-3.5 w-3.5"/>Print</Button>
        </div>
      </motion.div>

      {/* Summary cards */}
      {accounts.length > 0 && (
        <motion.div {...fadeUp} className="grid grid-cols-3 gap-3">
          <Card className="p-3 text-center">
            <p className="text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Total Debit</p>
            <p className="mt-0.5 text-lg font-bold tabular-nums text-red-600">₹{fmt(totalDebit)}</p>
          </Card>
          <Card className="p-3 text-center">
            <p className="text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Total Credit</p>
            <p className="mt-0.5 text-lg font-bold tabular-nums text-emerald-600">₹{fmt(totalCredit)}</p>
          </Card>
          <Card className="p-3 text-center">
            <p className="text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Difference</p>
            <p className={`mt-0.5 text-lg font-bold tabular-nums ${difference < 0.01 ? "text-emerald-600" : "text-red-600"}`}>
              {difference < 0.01 ? "✓ Balanced" : `₹${fmt(difference)}`}
            </p>
          </Card>
        </motion.div>
      )}

      <motion.div {...fadeUp}>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead className="w-10">#</TableHead>
              <TableHead>Account / Party</TableHead>
              <TableHead>Type</TableHead>
              <TableHead className="text-right">Debit (₹)</TableHead>
              <TableHead className="text-right">Credit (₹)</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            {accounts.length === 0 ? (
              <TableRow><TableCell colSpan={5} className="h-32 text-center"><p className="text-sm text-zinc-600 dark:text-zinc-300">{loading ? "Loading..." : "No data for this financial year"}</p></TableCell></TableRow>
            ) : (
              <>
                {accounts.map((a, i) => (
                  <TableRow key={i}>
                    <TableCell className="text-xs text-zinc-500 tabular-nums">{i + 1}</TableCell>
                    <TableCell className="font-medium">{a.name}</TableCell>
                    <TableCell className="text-xs text-zinc-500">{a.type}</TableCell>
                    <TableCell className="text-right font-mono text-red-600">{a.debit > 0 ? `₹${fmt(a.debit)}` : ""}</TableCell>
                    <TableCell className="text-right font-mono text-emerald-600">{a.credit > 0 ? `₹${fmt(a.credit)}` : ""}</TableCell>
                  </TableRow>
                ))}
                <TableRow className="border-t-2 border-zinc-300 dark:border-zinc-600 font-bold bg-zinc-50/50 dark:bg-zinc-800/50">
                  <TableCell></TableCell>
                  <TableCell className="font-bold">TOTAL</TableCell>
                  <TableCell></TableCell>
                  <TableCell className="text-right font-mono font-bold text-red-700">₹{fmt(totalDebit)}</TableCell>
                  <TableCell className="text-right font-mono font-bold text-emerald-700">₹{fmt(totalCredit)}</TableCell>
                </TableRow>
              </>
            )}
          </TableBody>
        </Table>
      </motion.div>
    </motion.div>
  );
}
