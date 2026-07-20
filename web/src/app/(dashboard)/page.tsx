"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { useApp } from "@/lib/app-context";
import { MetricCard } from "@/components/ui/metric-card";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { DashboardSkeleton } from "@/components/ui/skeleton";
import { TrendingUp, Wallet, Landmark, AlertCircle, IndianRupee } from "lucide-react";

interface DashboardData { todaySales: number; monthlySales: number; cashBalance: number; bankBalance: number; receivables: number; totalFunds: number; }
const fadeUp = { initial: { opacity: 0, y: 16 }, animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.5 } } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { style: "currency", currency: "INR", minimumFractionDigits: 2 }).format(n); }

export default function DashboardPage() {
  const { companyId, financialYear } = useApp();
  const [data, setData] = useState<DashboardData | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    setLoading(true);
    fetch(`/api/dashboard?companyId=${companyId}&financialYear=${financialYear}`)
      .then(r => r.json()).then(d => setData(d)).catch(() => {}).finally(() => setLoading(false));
  }, [companyId, financialYear]);

  if (loading) return <DashboardSkeleton />;
  const d = data || { todaySales: 0, monthlySales: 0, cashBalance: 0, bankBalance: 0, receivables: 0, totalFunds: 0 };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-6">
      <motion.div {...fadeUp}><h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Dashboard</h1><p className="mt-1 text-sm text-zinc-500">FY {financialYear} • Financial overview</p></motion.div>
      <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-5">
        <MetricCard title="Today's Sales" value={fmt(d.todaySales)} icon={TrendingUp} />
        <MetricCard title="Monthly Sales" value={fmt(d.monthlySales)} icon={IndianRupee} />
        <MetricCard title="Cash Balance" value={fmt(d.cashBalance)} icon={Wallet} />
        <MetricCard title="Bank + UPI" value={fmt(d.bankBalance)} icon={Landmark} />
        <MetricCard title="Receivables" value={fmt(d.receivables)} icon={AlertCircle} className="border-red-100 dark:border-red-900/30" />
      </div>
      <motion.div {...fadeUp}><Card><CardHeader><CardTitle>Quick Insights</CardTitle></CardHeader><CardContent><div className="grid grid-cols-2 gap-6 sm:grid-cols-4">
        <div><p className="text-xs text-zinc-500">Total Funds</p><p className="mt-1 text-lg font-semibold text-zinc-900 dark:text-zinc-100">{fmt(d.totalFunds)}</p></div>
        <div><p className="text-xs text-zinc-500">Cash Ratio</p><p className="mt-1 text-lg font-semibold text-zinc-900 dark:text-zinc-100">{d.totalFunds>0?Math.round((d.cashBalance/d.totalFunds)*100):0}%</p></div>
        <div><p className="text-xs text-zinc-500">Today</p><p className="mt-1 text-lg font-semibold text-zinc-900 dark:text-zinc-100">{fmt(d.todaySales)}</p></div>
        <div><p className="text-xs text-zinc-500">Outstanding</p><p className="mt-1 text-lg font-semibold text-red-500">{fmt(d.receivables)}</p></div>
      </div></CardContent></Card></motion.div>
    </motion.div>
  );
}
