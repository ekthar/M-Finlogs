"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { MetricCard } from "@/components/ui/metric-card";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import {
  TrendingUp,
  Wallet,
  Landmark,
  AlertCircle,
  IndianRupee,
} from "lucide-react";

interface DashboardData {
  todaySales: number;
  monthlySales: number;
  cashBalance: number;
  bankBalance: number;
  receivables: number;
  totalFunds: number;
}

const stagger = {
  animate: {
    transition: {
      staggerChildren: 0.06,
    },
  },
};

const fadeUp = {
  initial: { opacity: 0, y: 16 },
  animate: {
    opacity: 1,
    y: 0,
    transition: { type: "spring" as const, bounce: 0, duration: 0.5 },
  },
};

function fmt(n: number): string {
  return new Intl.NumberFormat("en-IN", {
    style: "currency",
    currency: "INR",
    minimumFractionDigits: 2,
  }).format(n);
}

export default function DashboardPage() {
  const [data, setData] = useState<DashboardData | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    fetch("/api/dashboard")
      .then((r) => r.json())
      .then((d) => setData(d))
      .catch(() => {})
      .finally(() => setLoading(false));
  }, []);

  const d = data || {
    todaySales: 0,
    monthlySales: 0,
    cashBalance: 0,
    bankBalance: 0,
    receivables: 0,
    totalFunds: 0,
  };

  return (
    <motion.div
      variants={stagger}
      initial="initial"
      animate="animate"
      className="space-y-6"
    >
      <motion.div {...fadeUp}>
        <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">
          Dashboard
        </h1>
        <p className="mt-1 text-sm text-zinc-500 dark:text-zinc-400">
          {loading ? "Loading..." : "Overview of your financial activity"}
        </p>
      </motion.div>

      <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-4 xl:grid-cols-5">
        <MetricCard title="Today's Sales" value={fmt(d.todaySales)} icon={TrendingUp} />
        <MetricCard title="Monthly Sales" value={fmt(d.monthlySales)} icon={IndianRupee} />
        <MetricCard title="Cash Balance" value={fmt(d.cashBalance)} icon={Wallet} />
        <MetricCard title="Bank Balance" value={fmt(d.bankBalance)} icon={Landmark} />
        <MetricCard
          title="Receivables"
          value={fmt(d.receivables)}
          icon={AlertCircle}
          className="border-red-100 dark:border-red-900/30"
        />
      </div>

      <motion.div {...fadeUp}>
        <Card>
          <CardHeader>
            <CardTitle>Quick Insights</CardTitle>
          </CardHeader>
          <CardContent>
            <div className="grid grid-cols-2 gap-6 sm:grid-cols-4">
              <div>
                <p className="text-xs text-zinc-500 dark:text-zinc-400">Total Funds</p>
                <p className="mt-1 text-lg font-semibold text-zinc-900 dark:text-zinc-100">{fmt(d.totalFunds)}</p>
              </div>
              <div>
                <p className="text-xs text-zinc-500 dark:text-zinc-400">Cash Ratio</p>
                <p className="mt-1 text-lg font-semibold text-zinc-900 dark:text-zinc-100">
                  {d.totalFunds > 0 ? Math.round((d.cashBalance / d.totalFunds) * 100) : 0}%
                </p>
              </div>
              <div>
                <p className="text-xs text-zinc-500 dark:text-zinc-400">Today Sales</p>
                <p className="mt-1 text-lg font-semibold text-zinc-900 dark:text-zinc-100">{fmt(d.todaySales)}</p>
              </div>
              <div>
                <p className="text-xs text-zinc-500 dark:text-zinc-400">Outstanding</p>
                <p className="mt-1 text-lg font-semibold text-red-500">{fmt(d.receivables)}</p>
              </div>
            </div>
          </CardContent>
        </Card>
      </motion.div>
    </motion.div>
  );
}
