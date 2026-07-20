"use client";

import { motion } from "framer-motion";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Select } from "@/components/ui/select";
import { Badge } from "@/components/ui/badge";
import {
  Table, TableHeader, TableBody, TableRow, TableHead, TableCell,
} from "@/components/ui/data-table";
import { Download, Bell, Search } from "lucide-react";

const fadeUp = {
  initial: { opacity: 0, y: 12 },
  animate: { opacity: 1, y: 0 },
  transition: { type: "spring" as const, bounce: 0, duration: 0.4 },
};

export default function OutstandingPage() {
  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Credit Outstanding</h1>
          <p className="mt-0.5 text-sm text-zinc-500">Track credit balances and overdue payments</p>
        </div>
        <div className="flex gap-2">
          <Button variant="outline" size="sm"><Bell className="mr-1.5 h-3.5 w-3.5" /> Alerts: 0</Button>
          <Button variant="outline" size="sm"><Download className="mr-1.5 h-3.5 w-3.5" /> Export</Button>
        </div>
      </motion.div>

      {/* Summary Cards */}
      <motion.div {...fadeUp} className="grid grid-cols-2 gap-3 sm:grid-cols-4">
        <Card className="p-4 text-center">
          <p className="text-xs font-medium text-zinc-500">Total Outstanding</p>
          <p className="mt-1 text-xl font-semibold text-red-500">₹0.00</p>
        </Card>
        <Card className="p-4 text-center">
          <p className="text-xs font-medium text-zinc-500">15+ Days Due</p>
          <p className="mt-1 text-xl font-semibold text-amber-500">0</p>
        </Card>
        <Card className="p-4 text-center">
          <p className="text-xs font-medium text-zinc-500">30+ Critical</p>
          <p className="mt-1 text-xl font-semibold text-red-600">0</p>
        </Card>
        <Card className="p-4 text-center">
          <p className="text-xs font-medium text-zinc-500">30+ Amount</p>
          <p className="mt-1 text-xl font-semibold text-red-600">₹0.00</p>
        </Card>
      </motion.div>

      {/* Filters */}
      <motion.div {...fadeUp}>
        <div className="flex flex-wrap items-center gap-3">
          <Select className="h-8 w-40 text-xs">
            <option value="risk-desc">Risk First</option>
            <option value="name-asc">A → Z</option>
            <option value="amount-desc">Amount ↓</option>
            <option value="days-desc">Days Unpaid ↓</option>
          </Select>
          <Select className="h-8 w-36 text-xs">
            <option value="all">All</option>
            <option value="high">15+ only</option>
            <option value="critical">30+ only</option>
          </Select>
          <div className="relative ml-auto">
            <Search className="absolute left-2.5 top-1/2 h-3.5 w-3.5 -translate-y-1/2 text-zinc-400" />
            <Input placeholder="Search..." className="h-8 w-48 pl-8 text-xs" />
          </div>
        </div>
      </motion.div>

      {/* Table */}
      <motion.div {...fadeUp}>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead>Party</TableHead>
              <TableHead>Status</TableHead>
              <TableHead className="text-right">Days Unpaid</TableHead>
              <TableHead>Last Receipt</TableHead>
              <TableHead className="text-right">Outstanding Balance</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            <TableRow>
              <TableCell colSpan={5} className="h-32 text-center">
                <div className="flex flex-col items-center gap-2">
                  <div className="text-3xl">✅</div>
                  <p className="text-sm text-zinc-500">No outstanding credits</p>
                </div>
              </TableCell>
            </TableRow>
          </TableBody>
        </Table>
      </motion.div>
    </motion.div>
  );
}
