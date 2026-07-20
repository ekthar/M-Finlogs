"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { Card, CardContent } from "@/components/ui/card";

const fadeUp = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { style: "currency", currency: "INR", minimumFractionDigits: 2 }).format(n); }

export default function ProfitLossPage() {
  const [sales, setSales] = useState(0);
  const [expenses, setExpenses] = useState(0);
  const [profit, setProfit] = useState(0);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    fetch("/api/reports/profit-loss").then(r => r.json()).then(d => {
      setSales(d.totalSales || 0);
      setExpenses(d.totalExpenses || 0);
      setProfit(d.netProfit || 0);
    }).catch(() => {}).finally(() => setLoading(false));
  }, []);

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp}><h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Profit & Loss</h1><p className="mt-0.5 text-sm text-zinc-500">{loading ? "Loading..." : "Revenue, expenses, and net profit"}</p></motion.div>

      <motion.div {...fadeUp}>
        <Card>
          <CardContent className="p-0">
            <table className="w-full text-sm">
              <tbody>
                <tr className="border-b border-zinc-100 dark:border-zinc-800"><td className="px-6 py-4 font-medium text-zinc-700 dark:text-zinc-300">Total Sales</td><td className="px-6 py-4 text-right font-semibold text-zinc-900 dark:text-zinc-100">{fmt(sales)}</td></tr>
                <tr className="border-b border-zinc-100 dark:border-zinc-800"><td className="px-6 py-4 font-medium text-zinc-700 dark:text-zinc-300">Total Expenses</td><td className="px-6 py-4 text-right font-semibold text-red-500">{fmt(expenses)}</td></tr>
                <tr className="bg-zinc-50/50 dark:bg-zinc-800/30"><td className="px-6 py-5 text-base font-semibold text-zinc-900 dark:text-zinc-100">Net Profit</td><td className={`px-6 py-5 text-right text-xl font-bold ${profit >= 0 ? "text-emerald-600" : "text-red-600"}`}>{fmt(profit)}</td></tr>
              </tbody>
            </table>
          </CardContent>
        </Card>
      </motion.div>
    </motion.div>
  );
}
