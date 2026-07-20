"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { useApp } from "@/lib/app-context";
import { MetricCard } from "@/components/ui/metric-card";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { DashboardSkeleton } from "@/components/ui/skeleton";
import { AnimatedNumber } from "@/components/ui/animated-number";
import { ActivityFeed } from "@/components/activity-feed";
import { springs } from "@/lib/design-tokens";
import { TrendingUp, Wallet, Landmark, AlertCircle, IndianRupee } from "lucide-react";
import { AreaChart, Area, XAxis, YAxis, Tooltip, ResponsiveContainer, PieChart, Pie, Cell } from "recharts";

interface DashboardData { todaySales: number; monthlySales: number; cashBalance: number; bankBalance: number; receivables: number; totalFunds: number; }
interface TrendPoint { date: string; sales: number; expenses: number; receipts: number; }
interface SparkData { sales: number[]; expenses: number[]; cash: number[]; receipts: number[]; }

const containerVariants = { initial: {}, animate: { transition: { staggerChildren: 0.06 } } };
const itemVariants = { initial: { opacity: 0, y: 20 }, animate: { opacity: 1, y: 0, transition: springs.default } };

const CHART_COLORS = ["#10b981", "#3b82f6", "#f59e0b", "#ef4444"];

export default function DashboardPage() {
  const { companyId, financialYear } = useApp();
  const [data, setData] = useState<DashboardData | null>(null);
  const [trend, setTrend] = useState<TrendPoint[]>([]);
  const [spark, setSpark] = useState<SparkData>({ sales: [], expenses: [], cash: [], receipts: [] });
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    setLoading(true);
    Promise.all([
      fetch(`/api/dashboard?companyId=${companyId}&financialYear=${financialYear}`).then(r => r.json()),
      fetch(`/api/dashboard/trend?companyId=${companyId}&days=30`).then(r => r.json()),
      fetch(`/api/dashboard/sparkline?companyId=${companyId}`).then(r => r.json()),
    ]).then(([d, t, s]) => {
      setData(d);
      setTrend(t.trend || []);
      setSpark(s || { sales: [], expenses: [], cash: [], receipts: [] });
    }).catch(() => {}).finally(() => setLoading(false));
  }, [companyId, financialYear]);

  if (loading) return <DashboardSkeleton />;
  const d = data || { todaySales: 0, monthlySales: 0, cashBalance: 0, bankBalance: 0, receivables: 0, totalFunds: 0 };

  const pieData = [
    { name: "Cash", value: Math.max(0, d.cashBalance) },
    { name: "Bank/UPI", value: Math.max(0, d.bankBalance) },
    { name: "Receivables", value: Math.max(0, d.receivables) },
  ].filter(p => p.value > 0);

  return (
    <motion.div variants={containerVariants} initial="initial" animate="animate" className="space-y-6">
      <motion.div variants={itemVariants}>
        <h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-100">Dashboard</h1>
        <p className="mt-1 text-sm text-zinc-500">FY {financialYear} overview</p>
      </motion.div>

      <motion.div variants={containerVariants} className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-5">
        <MetricCard title="Today's Sales" value={d.todaySales} icon={TrendingUp} sparkData={spark.sales} sparkColor="#10b981" />
        <MetricCard title="Monthly Sales" value={d.monthlySales} icon={IndianRupee} sparkData={spark.sales} sparkColor="#6366f1" />
        <MetricCard title="Cash Balance" value={d.cashBalance} icon={Wallet} sparkData={spark.cash} sparkColor="#f59e0b" />
        <MetricCard title="Bank + UPI" value={d.bankBalance} icon={Landmark} sparkData={spark.receipts} sparkColor="#3b82f6" />
        <MetricCard title="Receivables" value={d.receivables} icon={AlertCircle} className="border-red-100/50 dark:border-red-900/20" sparkData={spark.expenses} sparkColor="#ef4444" />
      </motion.div>

      {/* Charts Row */}
      <div className="grid grid-cols-1 gap-4 lg:grid-cols-3">
        {/* Sales Trend */}
        <motion.div variants={itemVariants} className="lg:col-span-2">
          <Card className="h-[340px]">
            <CardHeader><CardTitle>Sales Trend (30 Days)</CardTitle></CardHeader>
            <CardContent className="h-[260px] pr-2">
              {trend.length > 0 ? (
                <ResponsiveContainer width="100%" height="100%">
                  <AreaChart data={trend} margin={{ top: 5, right: 10, left: 0, bottom: 5 }}>
                    <defs>
                      <linearGradient id="salesGrad" x1="0" y1="0" x2="0" y2="1">
                        <stop offset="5%" stopColor="#10b981" stopOpacity={0.3}/>
                        <stop offset="95%" stopColor="#10b981" stopOpacity={0}/>
                      </linearGradient>
                    </defs>
                    <XAxis dataKey="date" tickFormatter={(d) => new Date(d).getDate().toString()} tick={{ fontSize: 11 }} axisLine={false} tickLine={false} />
                    <YAxis tickFormatter={(v) => `₹${(v/1000).toFixed(0)}k`} tick={{ fontSize: 11 }} axisLine={false} tickLine={false} width={50} />
                    <Tooltip formatter={(v) => [`₹${Number(v).toLocaleString("en-IN")}`, ""]} labelFormatter={(l) => new Date(l as string).toLocaleDateString("en-IN")} contentStyle={{ borderRadius: 12, border: "1px solid #e4e4e7", fontSize: 12 }} />
                    <Area type="monotone" dataKey="sales" stroke="#10b981" strokeWidth={2} fill="url(#salesGrad)" />
                  </AreaChart>
                </ResponsiveContainer>
              ) : (
                <div className="flex h-full items-center justify-center text-sm text-zinc-400">No trend data yet</div>
              )}
            </CardContent>
          </Card>
        </motion.div>

        {/* Fund Distribution Pie */}
        <motion.div variants={itemVariants}>
          <Card className="h-[340px]">
            <CardHeader><CardTitle>Fund Distribution</CardTitle></CardHeader>
            <CardContent className="h-[260px] flex items-center justify-center">
              {pieData.length > 0 ? (
                <ResponsiveContainer width="100%" height="100%">
                  <PieChart>
                    <Pie data={pieData} cx="50%" cy="50%" innerRadius={55} outerRadius={85} paddingAngle={3} dataKey="value" label={({ name, percent }) => `${name} ${((percent ?? 0)*100).toFixed(0)}%`} labelLine={false}>
                      {pieData.map((_, i) => <Cell key={i} fill={CHART_COLORS[i % CHART_COLORS.length]} />)}
                    </Pie>
                    <Tooltip formatter={(v) => [`₹${Number(v).toLocaleString("en-IN")}`, ""]} contentStyle={{ borderRadius: 12, border: "1px solid #e4e4e7", fontSize: 12 }} />
                  </PieChart>
                </ResponsiveContainer>
              ) : (
                <div className="text-sm text-zinc-400">No fund data</div>
              )}
            </CardContent>
          </Card>
        </motion.div>
      </div>

      {/* Quick Insights + Activity Feed */}
      <div className="grid grid-cols-1 gap-4 lg:grid-cols-3">
        <motion.div variants={itemVariants} className="lg:col-span-2">
          <Card>
            <CardHeader><CardTitle>Quick Insights</CardTitle></CardHeader>
            <CardContent>
              <div className="grid grid-cols-2 gap-8 sm:grid-cols-4">
                <div><p className="text-[11px] font-medium uppercase tracking-widest text-zinc-400">Total Funds</p><AnimatedNumber value={d.totalFunds} prefix="₹" className="mt-1.5 text-xl font-semibold text-zinc-900 dark:text-zinc-100" /></div>
                <div><p className="text-[11px] font-medium uppercase tracking-widest text-zinc-400">Cash Ratio</p><p className="mt-1.5 text-xl font-semibold text-zinc-900 dark:text-zinc-100">{d.totalFunds>0?Math.round((d.cashBalance/d.totalFunds)*100):0}%</p></div>
                <div><p className="text-[11px] font-medium uppercase tracking-widest text-zinc-400">Today</p><AnimatedNumber value={d.todaySales} prefix="₹" className="mt-1.5 text-xl font-semibold text-zinc-900 dark:text-zinc-100" /></div>
                <div><p className="text-[11px] font-medium uppercase tracking-widest text-zinc-400">Outstanding</p><AnimatedNumber value={d.receivables} prefix="₹" className="mt-1.5 text-xl font-semibold text-red-500" /></div>
              </div>
            </CardContent>
          </Card>
        </motion.div>

        <motion.div variants={itemVariants}>
          <Card className="h-full">
            <CardHeader><CardTitle>Recent Activity</CardTitle></CardHeader>
            <CardContent className="max-h-[200px] overflow-y-auto">
              <ActivityFeed limit={6} />
            </CardContent>
          </Card>
        </motion.div>
      </div>
    </motion.div>
  );
}
