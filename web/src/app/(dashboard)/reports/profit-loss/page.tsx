"use client";

import { motion } from "framer-motion";
import { Card, CardContent } from "@/components/ui/card";

const fadeUp = {
  initial: { opacity: 0, y: 12 },
  animate: { opacity: 1, y: 0 },
  transition: { type: "spring" as const, bounce: 0, duration: 0.4 },
};

export default function ProfitLossPage() {
  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp}>
        <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Profit & Loss</h1>
        <p className="mt-0.5 text-sm text-zinc-500">Revenue, expenses, and net profit overview</p>
      </motion.div>

      <motion.div {...fadeUp}>
        <Card>
          <CardContent className="p-0">
            <table className="w-full text-sm">
              <tbody>
                <tr className="border-b border-zinc-100 dark:border-zinc-800">
                  <td className="px-6 py-4 font-medium text-zinc-700 dark:text-zinc-300">Total Sales</td>
                  <td className="px-6 py-4 text-right font-semibold text-zinc-900 dark:text-zinc-100">₹0.00</td>
                </tr>
                <tr className="border-b border-zinc-100 dark:border-zinc-800">
                  <td className="px-6 py-4 font-medium text-zinc-700 dark:text-zinc-300">Total Expenses</td>
                  <td className="px-6 py-4 text-right font-semibold text-red-500">₹0.00</td>
                </tr>
                <tr className="bg-zinc-50/50 dark:bg-zinc-800/30">
                  <td className="px-6 py-5 text-base font-semibold text-zinc-900 dark:text-zinc-100">Net Profit</td>
                  <td className="px-6 py-5 text-right text-xl font-bold text-emerald-600 dark:text-emerald-400">₹0.00</td>
                </tr>
              </tbody>
            </table>
          </CardContent>
        </Card>
      </motion.div>
    </motion.div>
  );
}
