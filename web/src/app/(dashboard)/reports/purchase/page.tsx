"use client";

import { motion } from "framer-motion";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Select } from "@/components/ui/select";
import {
  Table, TableHeader, TableBody, TableRow, TableHead, TableCell,
} from "@/components/ui/data-table";
import { Download, Printer } from "lucide-react";

const fadeUp = {
  initial: { opacity: 0, y: 12 },
  animate: { opacity: 1, y: 0 },
  transition: { type: "spring" as const, bounce: 0, duration: 0.4 },
};

export default function PurchaseReportPage() {
  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Purchase Report</h1>
          <p className="mt-0.5 text-sm text-zinc-500">Monthly and party-wise purchase breakdown</p>
        </div>
        <div className="flex gap-2">
          <Button variant="outline" size="sm"><Download className="mr-1.5 h-3.5 w-3.5" /> Export</Button>
          <Button variant="outline" size="sm"><Printer className="mr-1.5 h-3.5 w-3.5" /> Print</Button>
        </div>
      </motion.div>

      <motion.div {...fadeUp}>
        <Card className="p-4">
          <div className="flex flex-wrap items-end gap-3">
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">From</label>
              <Input type="date" className="w-40" />
            </div>
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">To</label>
              <Input type="date" className="w-40" />
            </div>
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Party</label>
              <Input placeholder="Search party..." className="w-48" />
            </div>
            <Button variant="secondary" size="sm">Last 30 Days</Button>
            <Button size="sm">Apply</Button>
          </div>
        </Card>
      </motion.div>

      {/* Monthly Purchase */}
      <motion.div {...fadeUp}>
        <h3 className="mb-2 text-sm font-semibold text-zinc-700 dark:text-zinc-300">Monthly Purchase</h3>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead>Month</TableHead>
              <TableHead className="text-right">Total Purchase</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            <TableRow>
              <TableCell colSpan={2} className="h-20 text-center text-sm text-zinc-500">No data</TableCell>
            </TableRow>
          </TableBody>
        </Table>
      </motion.div>

      {/* Party-wise Purchase */}
      <motion.div {...fadeUp}>
        <div className="flex items-center justify-between mb-2">
          <h3 className="text-sm font-semibold text-zinc-700 dark:text-zinc-300">Party-wise Purchase</h3>
          <Select className="h-8 w-40 text-xs">
            <option value="name-asc">A → Z</option>
            <option value="amount-desc">Amount ↓</option>
          </Select>
        </div>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead>Party</TableHead>
              <TableHead className="text-right">Total Purchase</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            <TableRow>
              <TableCell colSpan={2} className="h-20 text-center text-sm text-zinc-500">No data</TableCell>
            </TableRow>
          </TableBody>
        </Table>
      </motion.div>
    </motion.div>
  );
}
