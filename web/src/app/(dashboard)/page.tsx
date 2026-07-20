"use client";

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

const stagger = {
  animate: {
    transition: {
      staggerChildren: 0.06,
    },
  },
};

const fadeUp = {
  initial: { opacity: 0, y: 16 },
  animate: { opacity: 1, y: 0 },
  transition: { type: "spring" as const, bounce: 0, duration: 0.5 },
};

export default function DashboardPage() {
  return (
    <motion.div
      variants={stagger}
      initial="initial"
      animate="animate"
      className="space-y-6"
    >
      {/* Page title */}
      <motion.div {...fadeUp}>
        <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">
          Dashboard
        </h1>
        <p className="mt-1 text-sm text-zinc-500 dark:text-zinc-400">
          Overview of your financial activity
        </p>
      </motion.div>

      {/* Metrics grid */}
      <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-4 xl:grid-cols-5">
        <MetricCard
          title="Today's Sales"
          value="₹0.00"
          icon={TrendingUp}
          trend={{ value: 0, label: "vs yesterday" }}
        />
        <MetricCard
          title="Monthly Sales"
          value="₹0.00"
          icon={IndianRupee}
          trend={{ value: 0, label: "this month" }}
        />
        <MetricCard
          title="Cash Balance"
          value="₹0.00"
          icon={Wallet}
        />
        <MetricCard
          title="Bank Balance"
          value="₹0.00"
          icon={Landmark}
        />
        <MetricCard
          title="Receivables"
          value="₹0.00"
          icon={AlertCircle}
          className="border-red-100 dark:border-red-900/30"
        />
      </div>

      {/* Charts row */}
      <div className="grid grid-cols-1 gap-4 lg:grid-cols-3">
        <motion.div {...fadeUp} className="lg:col-span-2">
          <Card className="h-[360px]">
            <CardHeader>
              <CardTitle>Sales Trend</CardTitle>
            </CardHeader>
            <CardContent className="flex h-[280px] items-center justify-center">
              <div className="text-center text-sm text-zinc-400">
                <div className="mb-2 text-4xl">📊</div>
                <p>Sales chart will render here</p>
                <p className="text-xs mt-1">Connect your database to see live data</p>
              </div>
            </CardContent>
          </Card>
        </motion.div>

        <motion.div {...fadeUp}>
          <Card className="h-[360px]">
            <CardHeader>
              <CardTitle>Fund Distribution</CardTitle>
            </CardHeader>
            <CardContent className="flex h-[280px] items-center justify-center">
              <div className="text-center text-sm text-zinc-400">
                <div className="mb-2 text-4xl">💰</div>
                <p>Cash vs Bank distribution</p>
                <p className="text-xs mt-1">Connect your database to see live data</p>
              </div>
            </CardContent>
          </Card>
        </motion.div>
      </div>

      {/* Quick insights */}
      <motion.div {...fadeUp}>
        <Card>
          <CardHeader>
            <CardTitle>Quick Insights</CardTitle>
          </CardHeader>
          <CardContent>
            <div className="grid grid-cols-2 gap-6 sm:grid-cols-4">
              <div>
                <p className="text-xs text-zinc-500 dark:text-zinc-400">Total Funds</p>
                <p className="mt-1 text-lg font-semibold text-zinc-900 dark:text-zinc-100">₹0.00</p>
              </div>
              <div>
                <p className="text-xs text-zinc-500 dark:text-zinc-400">Cash Ratio</p>
                <p className="mt-1 text-lg font-semibold text-zinc-900 dark:text-zinc-100">0%</p>
              </div>
              <div>
                <p className="text-xs text-zinc-500 dark:text-zinc-400">This Month Expenses</p>
                <p className="mt-1 text-lg font-semibold text-zinc-900 dark:text-zinc-100">₹0.00</p>
              </div>
              <div>
                <p className="text-xs text-zinc-500 dark:text-zinc-400">Active Parties</p>
                <p className="mt-1 text-lg font-semibold text-zinc-900 dark:text-zinc-100">0</p>
              </div>
            </div>
          </CardContent>
        </Card>
      </motion.div>
    </motion.div>
  );
}
