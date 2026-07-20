"use client";

import { motion } from "framer-motion";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Select } from "@/components/ui/select";
import { Badge } from "@/components/ui/badge";
import { Download, Upload, FileText, Mail, Save, Plus, Minus } from "lucide-react";

const fadeUp = {
  initial: { opacity: 0, y: 12 },
  animate: { opacity: 1, y: 0 },
  transition: { type: "spring" as const, bounce: 0, duration: 0.4 },
};

export default function InventoryPage() {
  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      {/* Header */}
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">
            Inventory Management
          </h1>
          <p className="mt-0.5 text-sm text-zinc-500">Track daily stock levels and purchases</p>
        </div>
        <div className="flex flex-wrap gap-2">
          <Button variant="outline" size="sm"><Upload className="mr-1.5 h-3.5 w-3.5" /> Import</Button>
          <Button variant="outline" size="sm"><Download className="mr-1.5 h-3.5 w-3.5" /> Template</Button>
          <Button variant="outline" size="sm"><FileText className="mr-1.5 h-3.5 w-3.5" /> PDF</Button>
          <Button variant="outline" size="sm"><Download className="mr-1.5 h-3.5 w-3.5" /> Excel</Button>
          <Button variant="outline" size="sm"><Mail className="mr-1.5 h-3.5 w-3.5" /> Email</Button>
          <Button size="sm"><Save className="mr-1.5 h-3.5 w-3.5" /> Save</Button>
        </div>
      </motion.div>

      {/* Toolbar */}
      <motion.div {...fadeUp}>
        <Card className="p-4">
          <div className="flex flex-wrap items-end gap-4">
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Month</label>
              <Select className="w-44">
                <option>January</option>
                <option>February</option>
                <option>March</option>
                <option>April</option>
                <option>May</option>
                <option>June</option>
                <option>July</option>
                <option>August</option>
                <option>September</option>
                <option>October</option>
                <option>November</option>
                <option>December</option>
              </Select>
            </div>
            <div className="flex items-end gap-2">
              <div>
                <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Add Product</label>
                <Input placeholder="Enter product name" className="w-56" />
              </div>
              <Button variant="secondary" size="sm"><Plus className="mr-1 h-3.5 w-3.5" /> Add</Button>
              <Button variant="ghost" size="sm"><Plus className="mr-1 h-3.5 w-3.5" /> Gap</Button>
              <Button variant="ghost" size="sm"><Minus className="mr-1 h-3.5 w-3.5" /> Clean</Button>
            </div>
            <div className="ml-auto">
              <Badge variant="info">01-04-2026 to 30-04-2026</Badge>
            </div>
          </div>
        </Card>
      </motion.div>

      {/* Summary */}
      <motion.div {...fadeUp}>
        <Card className="p-4">
          <div className="grid grid-cols-2 gap-4 sm:grid-cols-4">
            <div>
              <p className="text-xs text-zinc-500">Current Quantity</p>
              <p className="mt-0.5 text-lg font-semibold text-zinc-900 dark:text-zinc-100">0.00</p>
            </div>
            <div>
              <p className="text-xs text-zinc-500">Purchase In</p>
              <p className="mt-0.5 text-lg font-semibold text-zinc-900 dark:text-zinc-100">0.00</p>
            </div>
            <div>
              <p className="text-xs text-zinc-500">Avg Daily Movement</p>
              <p className="mt-0.5 text-lg font-semibold text-zinc-900 dark:text-zinc-100">0.0</p>
            </div>
            <div>
              <p className="text-xs text-zinc-500">Reorder Products</p>
              <p className="mt-0.5 text-lg font-semibold text-red-500">0</p>
            </div>
          </div>
        </Card>
      </motion.div>

      {/* Inventory Grid Placeholder */}
      <motion.div {...fadeUp}>
        <div className="overflow-auto rounded-xl border border-zinc-200/60 bg-white/70 backdrop-blur-xl dark:border-zinc-700/60 dark:bg-zinc-900/70">
          <div className="flex h-64 items-center justify-center">
            <div className="text-center">
              <div className="text-4xl mb-3">📦</div>
              <p className="text-sm text-zinc-500">Inventory grid will render here</p>
              <p className="text-xs text-zinc-400 mt-1">Add products and track daily quantities</p>
            </div>
          </div>
        </div>
      </motion.div>
    </motion.div>
  );
}
