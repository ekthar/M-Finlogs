"use client";

import { useState } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { exportToExcel } from "@/lib/export-excel";
import { Download, Printer, FileText, Calendar, BarChart3 } from "lucide-react";

interface SummaryRow { date: string; openingCash: number; cashIn: number; cashExp: number; cashNeeded: number; cashInHand: number; shortExcess: number; bank: number; credit: number; totalSales: number; }
interface MonthlyRow { month: string; label: string; sales: number; expenses: number; receipts: number; purchases: number; cashIn: number; cashOut: number; bank: number; credit: number; totalIn: number; totalOut: number; txnCount: number; netProfit: number; netCash: number; }

const fadeUp = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }

export default function DailySummaryPage() {
  const { companyId, financialYear } = useApp();
  const [tab, setTab] = useState<"daily" | "monthly">("daily");

  // Daily state
  const [start, setStart] = useState("");
  const [end, setEnd] = useState("");
  const [rows, setRows] = useState<SummaryRow[]>([]);
  const [loading, setLoading] = useState(false);

  // Monthly state
  const [monthlyRows, setMonthlyRows] = useState<MonthlyRow[]>([]);
  const [monthlyTotals, setMonthlyTotals] = useState<any>(null);
  const [monthlyLoading, setMonthlyLoading] = useState(false);

  const loadDaily = async (days?: number) => {
    setLoading(true);
    const params = new URLSearchParams();
    if (companyId) params.set("companyId", companyId);
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

  const loadMonthly = async () => {
    setMonthlyLoading(true);
    try {
      const params = new URLSearchParams({ companyId });
      if (financialYear) params.set("financialYear", financialYear);
      const res = await fetch(`/api/reports/monthly-summary?${params}`);
      const data = await res.json();
      setMonthlyRows(data.summary || []);
      setMonthlyTotals(data.totals || null);
    } catch { /* */ }
    setMonthlyLoading(false);
  };

  const handleDailyExcel = () => {
    if (!rows.length) return;
    const data = rows.map(r => ({ Date: new Date(r.date).toLocaleDateString("en-IN"), Opening: r.openingCash, "Cash In": r.cashIn, "Cash Exp": r.cashExp, Needed: r.cashNeeded, "In Hand": r.cashInHand, "Short/Excess": r.shortExcess, Bank: r.bank, Credit: r.credit, "Total Sales": r.totalSales }));
    exportToExcel(data, "Daily_Summary");
    toast.success("Excel exported");
  };

  const handleMonthlyExcel = () => {
    if (!monthlyRows.length) return;
    const data = monthlyRows.map(r => ({ Month: r.label, Sales: r.sales, Expenses: r.expenses, Receipts: r.receipts, "Cash In": r.cashIn, "Cash Out": r.cashOut, Bank: r.bank, Credit: r.credit, "Net Profit": r.netProfit, "Net Cash": r.netCash, Transactions: r.txnCount }));
    exportToExcel(data, `Monthly_Summary_FY${financialYear}`, "Monthly");
    toast.success("Excel exported");
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      {/* Header with tabs */}
      <motion.div {...fadeUp} className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-3">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-50">Summary Reports</h1>
          <p className="mt-1 text-sm text-zinc-600 dark:text-zinc-300">
            {tab === "daily" ? "Daily cash flow with opening/closing balances" : `Monthly breakdown • FY ${financialYear}`}
          </p>
        </div>
        <div className="flex items-center gap-2">
          {tab === "daily" && (
            <>
              <Button variant="outline" size="sm" onClick={handleDailyExcel} disabled={!rows.length}><Download className="mr-1.5 h-3.5 w-3.5"/>Excel</Button>
              <Button variant="outline" size="sm" onClick={() => window.print()}><Printer className="mr-1.5 h-3.5 w-3.5"/>Print</Button>
            </>
          )}
          {tab === "monthly" && (
            <Button variant="outline" size="sm" onClick={handleMonthlyExcel} disabled={!monthlyRows.length}><Download className="mr-1.5 h-3.5 w-3.5"/>Excel</Button>
          )}
        </div>
      </motion.div>

      {/* Tab switcher */}
      <motion.div {...fadeUp}>
        <div className="inline-flex rounded-lg border border-zinc-200 dark:border-zinc-700 p-0.5 bg-zinc-100/50 dark:bg-zinc-800/50">
          <button
            onClick={() => setTab("daily")}
            className={`flex items-center gap-1.5 rounded-md px-4 py-2 text-sm font-medium transition-all ${tab === "daily" ? "bg-white dark:bg-zinc-900 shadow-sm text-zinc-900 dark:text-zinc-100" : "text-zinc-500 hover:text-zinc-700 dark:hover:text-zinc-300"}`}
          >
            <Calendar className="h-3.5 w-3.5" /> Daily
          </button>
          <button
            onClick={() => { setTab("monthly"); if (!monthlyRows.length) loadMonthly(); }}
            className={`flex items-center gap-1.5 rounded-md px-4 py-2 text-sm font-medium transition-all ${tab === "monthly" ? "bg-white dark:bg-zinc-900 shadow-sm text-zinc-900 dark:text-zinc-100" : "text-zinc-500 hover:text-zinc-700 dark:hover:text-zinc-300"}`}
          >
            <BarChart3 className="h-3.5 w-3.5" /> Monthly
          </button>
        </div>
      </motion.div>

      {/* ═══════ DAILY TAB ═══════ */}
      {tab === "daily" && (
        <>
          <motion.div {...fadeUp}><Card className="p-4"><div className="flex flex-wrap items-end gap-3">
            <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">From</label><Input type="date" value={start} onChange={(e) => setStart(e.target.value)} className="w-40" /></div>
            <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">To</label><Input type="date" value={end} onChange={(e) => setEnd(e.target.value)} className="w-40" /></div>
            <Button variant="secondary" size="sm" onClick={() => loadDaily(30)}>Last 30 Days</Button>
            <Button variant="secondary" size="sm" onClick={() => loadDaily(90)}>Last 90 Days</Button>
            <Button size="sm" onClick={() => loadDaily()} disabled={loading}>{loading ? "..." : "Apply"}</Button>
          </div></Card></motion.div>

          <motion.div {...fadeUp}>
            <Table>
              <TableHeader><TableRow>
                <TableHead>Date</TableHead><TableHead className="text-right">Opening</TableHead><TableHead className="text-right">Cash In</TableHead><TableHead className="text-right">Cash Exp</TableHead><TableHead className="text-right">Needed</TableHead><TableHead className="text-right">In Hand</TableHead><TableHead className="text-right">Short/Excess</TableHead><TableHead className="text-right">Bank</TableHead><TableHead className="text-right">Credit</TableHead><TableHead className="text-right">Total Sales</TableHead>
              </TableRow></TableHeader>
              <TableBody>
                {rows.length === 0 ? (
                  <TableRow><TableCell colSpan={10} className="h-32 text-center"><p className="text-sm text-zinc-600 dark:text-zinc-300">Select a date range</p></TableCell></TableRow>
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
        </>
      )}

      {/* ═══════ MONTHLY TAB ═══════ */}
      {tab === "monthly" && (
        <>
          {/* Monthly Summary Cards */}
          {monthlyTotals && (
            <motion.div {...fadeUp} className="grid grid-cols-2 gap-3 sm:grid-cols-4">
              <Card className="p-3 text-center">
                <p className="text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Total Sales</p>
                <p className="mt-0.5 text-lg font-bold tabular-nums text-emerald-600">₹{fmt(monthlyTotals.sales)}</p>
              </Card>
              <Card className="p-3 text-center">
                <p className="text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Total Expenses</p>
                <p className="mt-0.5 text-lg font-bold tabular-nums text-red-600">₹{fmt(monthlyTotals.expenses)}</p>
              </Card>
              <Card className="p-3 text-center">
                <p className="text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Net Profit</p>
                <p className={`mt-0.5 text-lg font-bold tabular-nums ${monthlyTotals.netProfit >= 0 ? "text-emerald-600" : "text-red-600"}`}>₹{fmt(monthlyTotals.netProfit)}</p>
              </Card>
              <Card className="p-3 text-center">
                <p className="text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Transactions</p>
                <p className="mt-0.5 text-lg font-bold tabular-nums text-zinc-900 dark:text-zinc-50">{monthlyTotals.txnCount}</p>
              </Card>
            </motion.div>
          )}

          <motion.div {...fadeUp}>
            {monthlyLoading ? (
              <div className="flex items-center justify-center py-16"><div className="h-8 w-8 rounded-full border-4 border-zinc-200 border-t-zinc-900 animate-spin dark:border-zinc-700 dark:border-t-zinc-100" /></div>
            ) : (
              <Table>
                <TableHeader><TableRow>
                  <TableHead>Month</TableHead>
                  <TableHead className="text-right">Sales</TableHead>
                  <TableHead className="text-right">Expenses</TableHead>
                  <TableHead className="text-right">Receipts</TableHead>
                  <TableHead className="text-right">Cash In</TableHead>
                  <TableHead className="text-right">Cash Out</TableHead>
                  <TableHead className="text-right">Bank/UPI</TableHead>
                  <TableHead className="text-right">Credit</TableHead>
                  <TableHead className="text-right">Net Profit</TableHead>
                  <TableHead className="text-right">Txns</TableHead>
                </TableRow></TableHeader>
                <TableBody>
                  {monthlyRows.length === 0 ? (
                    <TableRow><TableCell colSpan={10} className="h-32 text-center"><p className="text-sm text-zinc-600 dark:text-zinc-300">No data for this financial year</p></TableCell></TableRow>
                  ) : (
                    <>
                      {monthlyRows.map((r) => (
                        <TableRow key={r.month}>
                          <TableCell className="font-medium">{r.label}</TableCell>
                          <TableCell className="text-right font-mono text-emerald-600">{fmt(r.sales)}</TableCell>
                          <TableCell className="text-right font-mono text-red-600">{fmt(r.expenses)}</TableCell>
                          <TableCell className="text-right font-mono">{fmt(r.receipts)}</TableCell>
                          <TableCell className="text-right font-mono">{fmt(r.cashIn)}</TableCell>
                          <TableCell className="text-right font-mono">{fmt(r.cashOut)}</TableCell>
                          <TableCell className="text-right font-mono">{fmt(r.bank)}</TableCell>
                          <TableCell className="text-right font-mono">{fmt(r.credit)}</TableCell>
                          <TableCell className={`text-right font-mono font-semibold ${r.netProfit >= 0 ? "text-emerald-600" : "text-red-600"}`}>{fmt(r.netProfit)}</TableCell>
                          <TableCell className="text-right tabular-nums">{r.txnCount}</TableCell>
                        </TableRow>
                      ))}
                      {/* Totals row */}
                      {monthlyTotals && (
                        <TableRow className="border-t-2 border-zinc-300 dark:border-zinc-600 font-bold bg-zinc-50/50 dark:bg-zinc-800/50">
                          <TableCell className="font-bold">TOTAL</TableCell>
                          <TableCell className="text-right font-mono font-bold text-emerald-700">₹{fmt(monthlyTotals.sales)}</TableCell>
                          <TableCell className="text-right font-mono font-bold text-red-700">₹{fmt(monthlyTotals.expenses)}</TableCell>
                          <TableCell className="text-right font-mono font-bold">{fmt(monthlyTotals.receipts)}</TableCell>
                          <TableCell className="text-right font-mono font-bold">{fmt(monthlyTotals.cashIn)}</TableCell>
                          <TableCell className="text-right font-mono font-bold">{fmt(monthlyTotals.cashOut)}</TableCell>
                          <TableCell className="text-right font-mono font-bold">{fmt(monthlyTotals.bank)}</TableCell>
                          <TableCell className="text-right font-mono font-bold">{fmt(monthlyTotals.credit)}</TableCell>
                          <TableCell className={`text-right font-mono font-bold ${monthlyTotals.netProfit >= 0 ? "text-emerald-700" : "text-red-700"}`}>₹{fmt(monthlyTotals.netProfit)}</TableCell>
                          <TableCell className="text-right font-bold tabular-nums">{monthlyTotals.txnCount}</TableCell>
                        </TableRow>
                      )}
                    </>
                  )}
                </TableBody>
              </Table>
            )}
          </motion.div>
        </>
      )}
    </motion.div>
  );
}
