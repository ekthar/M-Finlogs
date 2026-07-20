"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { useApp } from "@/lib/app-context";
import { MetricCard } from "@/components/ui/metric-card";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { DashboardSkeleton } from "@/components/ui/skeleton";
import { AnimatedNumber } from "@/components/ui/animated-number";
import { springs, stagger } from "@/lib/design-tokens";
import { TrendingUp, Wallet, Landmark, AlertCircle, IndianRupee } from "lucide-react";

interface DashboardData { todaySales: number; monthlySales: number; cashBalance: number; bankBalance: number; receivables: number; totalFunds: number; }

const containerVariants = { initial: {}, animate: { transition: { staggerChildren: stagger.default } } };
const itemVariants = { initial: { opacity: 0, y: 20 }, animate: { opacity: 1, y: 0, transition: springs.default } };

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
    <motion.div variants={containerVariants} initial="initial" animate="animate" className="space-y-6">
      <motion.div variants={itemVariants}>
        <h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-100">Dashboard</h1>
        <p className="mt-1 text-sm text-zinc-500">FY {financialYear} overview</p>
      </motion.div>

      <motion.div variants={containerVariants} className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-5">
        <MetricCard title="Today's Sales" value={d.todaySales} icon={TrendingUp} />
        <MetricCard title="Monthly Sales" value={d.monthlySales} icon={IndianRupee} />
        <MetricCard title="Cash Balance" value={d.cashBalance} icon={Wallet} />
        <MetricCard title="Bank + UPI" value={d.bankBalance} icon={Landmark} />
        <MetricCard title="Receivables" value={d.receivables} icon={AlertCircle} className="border-red-100/50 dark:border-red-900/20" />
      </motion.div>

      <motion.div variants={itemVariants}>
        <Card>
          <CardHeader><CardTitle>Quick Insights</CardTitle></CardHeader>
          <CardContent>
            <div className="grid grid-cols-2 gap-8 sm:grid-cols-4">
              <div>
                <p className="text-[11px] font-medium uppercase tracking-widest text-zinc-400">Total Funds</p>
                <AnimatedNumber value={d.totalFunds} prefix="₹" className="mt-1.5 text-xl font-semibold text-zinc-900 dark:text-zinc-100" />
              </div>
              <div>
                <p className="text-[11px] font-medium uppercase tracking-widest text-zinc-400">Cash Ratio</p>
                <p className="mt-1.5 text-xl font-semibold text-zinc-900 dark:text-zinc-100">{d.totalFunds>0?Math.round((d.cashBalance/d.totalFunds)*100):0}%</p>
              </div>
              <div>
                <p className="text-[11px] font-medium uppercase tracking-widest text-zinc-400">Today</p>
                <AnimatedNumber value={d.todaySales} prefix="₹" className="mt-1.5 text-xl font-semibold text-zinc-900 dark:text-zinc-100" />
              </div>
              <div>
                <p className="text-[11px] font-medium uppercase tracking-widest text-zinc-400">Outstanding</p>
                <AnimatedNumber value={d.receivables} prefix="₹" className="mt-1.5 text-xl font-semibold text-red-500" />
              </div>
            </div>
          </CardContent>
        </Card>
      </motion.div>
    </motion.div>
  );
}
