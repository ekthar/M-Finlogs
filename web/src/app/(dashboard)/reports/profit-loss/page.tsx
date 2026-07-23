"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { exportProfitLossPdf } from "@/lib/export-pdf";
import { exportToExcel } from "@/lib/export-excel";
import { Card, CardContent } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { FileText, Download, Printer } from "lucide-react";

const fadeUp = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { style: "currency", currency: "INR", minimumFractionDigits: 2 }).format(n); }

interface PnLData { totalSales: number; totalReturns: number; revenue: number; totalPurchases: number; grossProfit: number; totalExpenses: number; netProfit: number; totalReceipts: number; }

export default function ProfitLossPage() {
  const { companyId, financialYear } = useApp();
  const [data, setData] = useState<PnLData | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    fetch(`/api/reports/profit-loss?companyId=${companyId}&financialYear=${financialYear}`)
      .then(r => r.json())
      .then(d => setData(d))
      .catch(() => {})
      .finally(() => setLoading(false));
  }, [companyId, financialYear]);

  const d = data || { totalSales: 0, totalReturns: 0, revenue: 0, totalPurchases: 0, grossProfit: 0, totalExpenses: 0, netProfit: 0, totalReceipts: 0 };

  const handlePdf = () => { if (data) { exportProfitLossPdf(data); toast.success("PDF exported"); } };
  const handleExcel = () => {
    if (!data) return;
    exportToExcel([
      { Particulars: "Total Sales", "Amount (₹)": d.totalSales },
      { Particulars: "Less: Sale Returns", "Amount (₹)": -d.totalReturns },
      { Particulars: "Revenue (Net Sales)", "Amount (₹)": d.revenue },
      { Particulars: "Less: Purchases (COGS)", "Amount (₹)": -d.totalPurchases },
      { Particulars: "Gross Profit", "Amount (₹)": d.grossProfit },
      { Particulars: "Less: Expenses", "Amount (₹)": -d.totalExpenses },
      { Particulars: "Net Profit / (Loss)", "Amount (₹)": d.netProfit },
    ], `PnL_FY${financialYear}`, "Profit & Loss");
    toast.success("Excel exported");
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5 max-w-3xl">
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-50">Profit & Loss</h1>
          <p className="mt-1 text-sm text-zinc-600 dark:text-zinc-300">{loading ? "Loading..." : `FY ${financialYear}`}</p>
        </div>
        <div className="flex gap-2">
          <Button variant="outline" size="sm" onClick={handlePdf} disabled={!data}><FileText className="mr-1.5 h-3.5 w-3.5"/>PDF</Button>
          <Button variant="outline" size="sm" onClick={handleExcel} disabled={!data}><Download className="mr-1.5 h-3.5 w-3.5"/>Excel</Button>
          <Button variant="outline" size="sm" onClick={() => window.print()}><Printer className="mr-1.5 h-3.5 w-3.5"/>Print</Button>
        </div>
      </motion.div>

      <motion.div {...fadeUp}>
        <Card>
          <CardContent className="p-0">
            <table className="w-full text-sm">
              <thead>
                <tr className="border-b border-zinc-200 dark:border-zinc-700 bg-zinc-50/80 dark:bg-zinc-800/60">
                  <th className="px-6 py-3 text-left text-xs font-semibold uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Particulars</th>
                  <th className="px-6 py-3 text-right text-xs font-semibold uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Amount (₹)</th>
                </tr>
              </thead>
              <tbody>
                <tr className="border-b border-zinc-100 dark:border-zinc-800"><td className="px-6 py-3.5 font-medium text-zinc-700 dark:text-zinc-200">Total Sales</td><td className="px-6 py-3.5 text-right font-mono font-semibold text-zinc-900 dark:text-zinc-100">{fmt(d.totalSales)}</td></tr>
                {d.totalReturns > 0 && <tr className="border-b border-zinc-100 dark:border-zinc-800"><td className="px-6 py-3.5 text-zinc-600 dark:text-zinc-400 pl-10">Less: Sale Returns</td><td className="px-6 py-3.5 text-right font-mono text-red-500">-{fmt(d.totalReturns)}</td></tr>}
                <tr className="border-b border-zinc-200 dark:border-zinc-700 bg-zinc-50/30 dark:bg-zinc-800/20"><td className="px-6 py-3.5 font-semibold text-zinc-800 dark:text-zinc-200">Revenue (Net Sales)</td><td className="px-6 py-3.5 text-right font-mono font-semibold text-zinc-900 dark:text-zinc-100">{fmt(d.revenue)}</td></tr>
                {d.totalPurchases > 0 && <tr className="border-b border-zinc-100 dark:border-zinc-800"><td className="px-6 py-3.5 text-zinc-600 dark:text-zinc-400 pl-10">Less: Purchases (COGS)</td><td className="px-6 py-3.5 text-right font-mono text-red-500">-{fmt(d.totalPurchases)}</td></tr>}
                <tr className="border-b border-zinc-200 dark:border-zinc-700 bg-zinc-50/30 dark:bg-zinc-800/20"><td className="px-6 py-3.5 font-semibold text-zinc-800 dark:text-zinc-200">Gross Profit</td><td className={`px-6 py-3.5 text-right font-mono font-semibold ${d.grossProfit >= 0 ? "text-emerald-600" : "text-red-600"}`}>{fmt(d.grossProfit)}</td></tr>
                <tr className="border-b border-zinc-100 dark:border-zinc-800"><td className="px-6 py-3.5 text-zinc-600 dark:text-zinc-400 pl-10">Less: Expenses</td><td className="px-6 py-3.5 text-right font-mono text-red-500">-{fmt(d.totalExpenses)}</td></tr>
                <tr className="bg-zinc-100/60 dark:bg-zinc-800/50"><td className="px-6 py-5 text-base font-bold text-zinc-900 dark:text-zinc-50">Net {d.netProfit >= 0 ? "Profit" : "Loss"}</td><td className={`px-6 py-5 text-right text-xl font-bold tabular-nums ${d.netProfit >= 0 ? "text-emerald-600" : "text-red-600"}`}>{fmt(d.netProfit)}</td></tr>
              </tbody>
            </table>
          </CardContent>
        </Card>
      </motion.div>
    </motion.div>
  );
}
