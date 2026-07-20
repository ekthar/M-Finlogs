"use client";

import { useState } from "react";
import { motion } from "framer-motion";
import { Card, CardContent } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Select } from "@/components/ui/select";
import { Badge } from "@/components/ui/badge";
import {
  Table,
  TableHeader,
  TableBody,
  TableRow,
  TableHead,
  TableCell,
} from "@/components/ui/data-table";
import { Plus, ChevronLeft, ChevronRight, Search, Pencil, Trash2 } from "lucide-react";

const fadeUp = {
  initial: { opacity: 0, y: 12 },
  animate: { opacity: 1, y: 0 },
  transition: { type: "spring" as const, bounce: 0, duration: 0.4 },
};

export default function DailyEntryPage() {
  const [currentPage, setCurrentPage] = useState(1);

  return (
    <motion.div
      initial="initial"
      animate="animate"
      className="space-y-5"
    >
      {/* Header */}
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">
            Daily Transactions
          </h1>
          <p className="mt-0.5 text-sm text-zinc-500 dark:text-zinc-400">
            Add and manage daily entries
          </p>
        </div>
        <div className="flex items-center gap-2">
          <Badge variant="info">Page {currentPage}</Badge>
        </div>
      </motion.div>

      {/* Entry Form */}
      <motion.div {...fadeUp}>
        <Card className="p-4">
          <div className="grid grid-cols-1 gap-3 sm:grid-cols-2 lg:grid-cols-6 items-end">
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">
                Date
              </label>
              <Input type="date" defaultValue={new Date().toISOString().split("T")[0]} />
            </div>
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">
                Bill / Ref
              </label>
              <Input placeholder="Bill No" />
            </div>
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">
                Party
              </label>
              <Input placeholder="Select Party" list="party-list" defaultValue="Customer" />
            </div>
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">
                Type
              </label>
              <Select>
                <option>Sale</option>
                <option>Sale Return</option>
                <option>Expense</option>
                <option>Receipt</option>
              </Select>
            </div>
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">
                Mode
              </label>
              <Select>
                <option>Credit</option>
                <option>Cash</option>
                <option>UPI</option>
                <option>Bank</option>
              </Select>
            </div>
            <div className="flex gap-2">
              <div className="flex-1">
                <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">
                  Amount
                </label>
                <Input type="number" placeholder="0.00" min="0" step="1" />
              </div>
              <Button className="mt-auto h-10 w-10 p-0" size="icon">
                <Plus className="h-4 w-4" />
              </Button>
            </div>
          </div>
          <p className="mt-2 text-right text-[11px] text-zinc-400">
            Press <kbd className="rounded bg-zinc-100 px-1.5 py-0.5 text-[10px] font-medium dark:bg-zinc-800">Enter</kbd> in Amount to save
          </p>
        </Card>
      </motion.div>

      {/* Filters & Pagination */}
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <Button variant="secondary" size="sm">Last 30 Days</Button>
          <Button variant="ghost" size="sm">All</Button>
        </div>
        <div className="flex items-center gap-2">
          <div className="relative">
            <Search className="absolute left-2.5 top-1/2 h-3.5 w-3.5 -translate-y-1/2 text-zinc-400" />
            <Input placeholder="Search..." className="h-8 w-48 pl-8 text-xs" />
          </div>
          <div className="flex items-center gap-1">
            <Button
              variant="outline"
              size="icon"
              className="h-8 w-8"
              onClick={() => setCurrentPage(Math.max(1, currentPage - 1))}
            >
              <ChevronLeft className="h-4 w-4" />
            </Button>
            <span className="min-w-[60px] text-center text-xs text-zinc-500">
              Page {currentPage}
            </span>
            <Button
              variant="outline"
              size="icon"
              className="h-8 w-8"
              onClick={() => setCurrentPage(currentPage + 1)}
            >
              <ChevronRight className="h-4 w-4" />
            </Button>
          </div>
        </div>
      </motion.div>

      {/* Transactions Table */}
      <motion.div {...fadeUp}>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead>Date</TableHead>
              <TableHead>Bill No</TableHead>
              <TableHead>Party</TableHead>
              <TableHead>Type</TableHead>
              <TableHead>Mode</TableHead>
              <TableHead className="text-right">Amount</TableHead>
              <TableHead className="w-20 text-center">Actions</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            {/* Empty state */}
            <TableRow>
              <TableCell colSpan={7} className="h-32 text-center">
                <div className="flex flex-col items-center justify-center gap-2">
                  <div className="text-3xl">📋</div>
                  <p className="text-sm text-zinc-500">No transactions yet</p>
                  <p className="text-xs text-zinc-400">Add your first entry using the form above</p>
                </div>
              </TableCell>
            </TableRow>
          </TableBody>
        </Table>
      </motion.div>
    </motion.div>
  );
}
