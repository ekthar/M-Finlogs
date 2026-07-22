"use client";

import { motion } from "framer-motion";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Select } from "@/components/ui/select";
import {
  Table, TableHeader, TableBody, TableRow, TableHead, TableCell,
} from "@/components/ui/data-table";
import { Download } from "lucide-react";

const fadeUp = {
  initial: { opacity: 0, y: 12 },
  animate: { opacity: 1, y: 0 },
  transition: { type: "spring" as const, bounce: 0, duration: 0.4 },
};

export default function StockValuePage() {
  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-50">Stock Value Report</h1>
          <p className="mt-1 text-sm text-zinc-600 dark:text-zinc-300">Daily stock value based on cost price</p>
        </div>
        <div className="flex items-center gap-3">
          <Select className="h-9 w-44 text-sm">
            <option>July 2026</option>
            <option>June 2026</option>
          </Select>
          <Button variant="outline" size="sm"><Download className="mr-1.5 h-3.5 w-3.5" /> Export</Button>
        </div>
      </motion.div>

      <motion.div {...fadeUp}>
        <Card className="p-5 text-center">
          <p className="text-sm font-medium text-zinc-600 dark:text-zinc-300">Total Stock Value</p>
          <p className="mt-1.5 text-3xl font-bold tabular-nums text-zinc-900 dark:text-zinc-50">₹0.00</p>
        </Card>
      </motion.div>

      <motion.div {...fadeUp}>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead>Day</TableHead>
              <TableHead className="text-right">Total Qty</TableHead>
              <TableHead className="text-right">Stock Value</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            <TableRow>
              <TableCell colSpan={3} className="h-32 text-center">
                <div className="flex flex-col items-center gap-2">
                  <div className="text-3xl">📈</div>
                  <p className="text-sm text-zinc-600 dark:text-zinc-300">No stock value data available</p>
                </div>
              </TableCell>
            </TableRow>
          </TableBody>
        </Table>
      </motion.div>
    </motion.div>
  );
}
