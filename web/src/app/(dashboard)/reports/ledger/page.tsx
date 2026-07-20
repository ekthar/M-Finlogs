"use client";

import { motion } from "framer-motion";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import {
  Table,
  TableHeader,
  TableBody,
  TableRow,
  TableHead,
  TableCell,
} from "@/components/ui/data-table";
import { Download, Printer, Search } from "lucide-react";

const fadeUp = {
  initial: { opacity: 0, y: 12 },
  animate: { opacity: 1, y: 0 },
  transition: { type: "spring" as const, bounce: 0, duration: 0.4 },
};

export default function LedgerPage() {
  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">
            Party Ledger
          </h1>
          <p className="mt-0.5 text-sm text-zinc-500">Account-wise debit/credit statement</p>
        </div>
        <div className="flex gap-2">
          <Button variant="outline" size="sm">
            <Download className="mr-1.5 h-3.5 w-3.5" /> Export
          </Button>
          <Button variant="outline" size="sm">
            <Printer className="mr-1.5 h-3.5 w-3.5" /> Print
          </Button>
        </div>
      </motion.div>

      {/* Filters */}
      <motion.div {...fadeUp}>
        <Card className="p-4">
          <div className="flex flex-wrap items-end gap-4">
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Party</label>
              <Input placeholder="Search party..." className="w-52" />
            </div>
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">From</label>
              <Input type="date" className="w-40" />
            </div>
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">To</label>
              <Input type="date" className="w-40" />
            </div>
            <Button size="sm">Show</Button>
            <div className="ml-auto relative">
              <Search className="absolute left-2.5 top-1/2 h-3.5 w-3.5 -translate-y-1/2 text-zinc-400" />
              <Input placeholder="Search transactions..." className="h-9 w-52 pl-8 text-xs" />
            </div>
          </div>
        </Card>
      </motion.div>

      {/* Table */}
      <motion.div {...fadeUp}>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead>Date</TableHead>
              <TableHead>Bill</TableHead>
              <TableHead>Particulars</TableHead>
              <TableHead className="text-right">Debit</TableHead>
              <TableHead className="text-right">Credit</TableHead>
              <TableHead className="text-right">Balance</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            <TableRow>
              <TableCell colSpan={6} className="h-32 text-center">
                <div className="flex flex-col items-center gap-2">
                  <div className="text-3xl">📖</div>
                  <p className="text-sm text-zinc-500">Select a party to view ledger</p>
                </div>
              </TableCell>
            </TableRow>
          </TableBody>
        </Table>
      </motion.div>
    </motion.div>
  );
}
