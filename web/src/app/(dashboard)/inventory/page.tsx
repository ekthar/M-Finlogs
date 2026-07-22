"use client";

import { useState } from "react";
import { motion } from "framer-motion";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Select } from "@/components/ui/select";
import { Badge } from "@/components/ui/badge";
import { FullScreenWrapper } from "@/components/full-screen-wrapper";
import {
  Table, TableHeader, TableBody, TableRow, TableHead, TableCell,
} from "@/components/ui/data-table";
import {
  Download, Upload, FileText, Mail, Save, Plus, Minus, Maximize2,
} from "lucide-react";

const fadeUp = {
  initial: { opacity: 0, y: 12 },
  animate: { opacity: 1, y: 0 },
  transition: { type: "spring" as const, bounce: 0, duration: 0.4 },
};

// Generate days 1-31 for the inventory grid
const DAYS = Array.from({ length: 31 }, (_, i) => i + 1);

// Demo products for layout demonstration
const DEMO_PRODUCTS = [
  { name: "Amul Butter 500g", unit: "pcs" },
  { name: "Tata Salt 1kg", unit: "pcs" },
  { name: "Fortune Oil 1L", unit: "btl" },
  { name: "Britannia Bread", unit: "pkt" },
  { name: "Parle-G 250g", unit: "pkt" },
];

export default function InventoryPage() {
  const [month, setMonth] = useState("July");
  const [pdfLoading, setPdfLoading] = useState(false);
  const [excelLoading, setExcelLoading] = useState(false);

  const handlePdfExport = async () => {
    setPdfLoading(true);
    try {
      const { exportInventoryPdf } = await import("@/lib/export-pdf");
      await exportInventoryPdf({
        title: `Inventory Report - ${month} 2026`,
        products: DEMO_PRODUCTS,
        days: DAYS,
        data: [],
      });
    } catch (e) {
      console.error("PDF export failed:", e);
    }
    setPdfLoading(false);
  };

  const handleExcelExport = async () => {
    setExcelLoading(true);
    try {
      const { exportInventoryExcel } = await import("@/lib/export-excel");
      await exportInventoryExcel({
        title: `Inventory - ${month} 2026`,
        products: DEMO_PRODUCTS,
        days: DAYS,
        data: [],
      });
    } catch (e) {
      console.error("Excel export failed:", e);
    }
    setExcelLoading(false);
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      {/* Header */}
      <motion.div {...fadeUp} className="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-50">
            Inventory Management
          </h1>
          <p className="mt-1 text-sm text-zinc-600 dark:text-zinc-300">
            Track daily stock levels and purchases
          </p>
        </div>
        <div className="flex flex-wrap items-center gap-2">
          <Button variant="outline" size="sm">
            <Upload className="mr-1.5 h-3.5 w-3.5" /> Import
          </Button>
          <Button variant="outline" size="sm">
            <Download className="mr-1.5 h-3.5 w-3.5" /> Template
          </Button>
          <Button
            variant="outline"
            size="sm"
            onClick={handlePdfExport}
            disabled={pdfLoading}
          >
            <FileText className="mr-1.5 h-3.5 w-3.5" />
            {pdfLoading ? "Generating..." : "PDF"}
          </Button>
          <Button
            variant="outline"
            size="sm"
            onClick={handleExcelExport}
            disabled={excelLoading}
          >
            <Download className="mr-1.5 h-3.5 w-3.5" />
            {excelLoading ? "Generating..." : "Excel"}
          </Button>
          <Button variant="outline" size="sm">
            <Mail className="mr-1.5 h-3.5 w-3.5" /> Email
          </Button>
          <Button size="sm">
            <Save className="mr-1.5 h-3.5 w-3.5" /> Save
          </Button>
        </div>
      </motion.div>

      {/* Toolbar */}
      <motion.div {...fadeUp}>
        <Card className="p-4">
          <div className="flex flex-wrap items-end gap-4">
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">
                Month
              </label>
              <Select className="w-44" value={month} onChange={(e) => setMonth(e.target.value)}>
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
                <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">
                  Add Product
                </label>
                <Input placeholder="Enter product name" className="w-56" />
              </div>
              <Button variant="secondary" size="sm">
                <Plus className="mr-1 h-3.5 w-3.5" /> Add
              </Button>
              <Button variant="ghost" size="sm">
                <Plus className="mr-1 h-3.5 w-3.5" /> Gap
              </Button>
              <Button variant="ghost" size="sm">
                <Minus className="mr-1 h-3.5 w-3.5" /> Clean
              </Button>
            </div>
            <div className="ml-auto flex items-center gap-2">
              <Badge variant="info">01-07-2026 to 31-07-2026</Badge>
            </div>
          </div>
        </Card>
      </motion.div>

      {/* Summary */}
      <motion.div {...fadeUp}>
        <Card className="p-4">
          <div className="grid grid-cols-2 gap-4 sm:grid-cols-4">
            <div>
              <p className="text-sm font-medium text-zinc-600 dark:text-zinc-300">Current Quantity</p>
              <p className="mt-0.5 text-xl font-bold tabular-nums text-zinc-900 dark:text-zinc-50">0.00</p>
            </div>
            <div>
              <p className="text-sm font-medium text-zinc-600 dark:text-zinc-300">Purchase In</p>
              <p className="mt-0.5 text-xl font-bold tabular-nums text-zinc-900 dark:text-zinc-50">0.00</p>
            </div>
            <div>
              <p className="text-sm font-medium text-zinc-600 dark:text-zinc-300">Avg Daily Movement</p>
              <p className="mt-0.5 text-xl font-bold tabular-nums text-zinc-900 dark:text-zinc-50">0.0</p>
            </div>
            <div>
              <p className="text-sm font-medium text-zinc-600 dark:text-zinc-300">Reorder Products</p>
              <p className="mt-0.5 text-xl font-bold tabular-nums text-red-600 dark:text-red-400">0</p>
            </div>
          </div>
        </Card>
      </motion.div>

      {/* Full-Screen Inventory Grid */}
      <motion.div {...fadeUp}>
        <FullScreenWrapper className="mt-0">
          <div className="overflow-auto rounded-xl border border-zinc-200/60 bg-white dark:border-zinc-700/60 dark:bg-zinc-900 scrollbar-thin">
            <table className="w-full border-collapse text-sm">
              {/* Sticky header */}
              <thead className="sticky top-0 z-10 bg-zinc-50/95 backdrop-blur-sm dark:bg-zinc-800/95">
                <tr className="border-b border-zinc-200 dark:border-zinc-700">
                  <th className="sticky left-0 z-20 min-w-[180px] bg-zinc-100 px-3 py-3 text-left text-xs font-semibold uppercase tracking-wider text-zinc-700 dark:bg-zinc-800 dark:text-zinc-200">
                    Product
                  </th>
                  <th className="min-w-[60px] px-2 py-3 text-center text-xs font-semibold uppercase tracking-wider text-zinc-700 dark:text-zinc-200">
                    Unit
                  </th>
                  {DAYS.map((day) => (
                    <th
                      key={day}
                      className="min-w-[52px] px-1.5 py-3 text-center text-xs font-semibold tabular-nums text-zinc-600 dark:text-zinc-300"
                    >
                      {day}
                    </th>
                  ))}
                  <th className="min-w-[80px] px-3 py-3 text-right text-xs font-semibold uppercase tracking-wider text-zinc-700 dark:text-zinc-200">
                    Total
                  </th>
                </tr>
              </thead>
              <tbody>
                {DEMO_PRODUCTS.length === 0 ? (
                  <tr>
                    <td colSpan={34} className="h-48 text-center">
                      <div className="flex flex-col items-center gap-2 py-12">
                        <div className="text-4xl">📦</div>
                        <p className="text-sm font-medium text-zinc-600 dark:text-zinc-300">
                          No products added yet
                        </p>
                        <p className="text-sm text-zinc-500 dark:text-zinc-400">
                          Use the toolbar above to add products
                        </p>
                      </div>
                    </td>
                  </tr>
                ) : (
                  DEMO_PRODUCTS.map((product, idx) => (
                    <tr
                      key={idx}
                      className="border-b border-zinc-100 transition-colors hover:bg-zinc-50/80 dark:border-zinc-800 dark:hover:bg-zinc-800/50"
                    >
                      <td className="sticky left-0 z-10 bg-white px-3 py-2.5 text-sm font-medium text-zinc-900 dark:bg-zinc-900 dark:text-zinc-100">
                        {product.name}
                      </td>
                      <td className="px-2 py-2.5 text-center text-xs text-zinc-500 dark:text-zinc-400">
                        {product.unit}
                      </td>
                      {DAYS.map((day) => (
                        <td key={day} className="px-1 py-1.5 text-center">
                          <input
                            type="number"
                            className="h-8 w-full rounded-md border border-zinc-200/60 bg-transparent px-1 text-center text-sm tabular-nums text-zinc-700 placeholder:text-zinc-300 focus:border-zinc-400 focus:outline-none focus:ring-1 focus:ring-zinc-300 dark:border-zinc-700/60 dark:text-zinc-200 dark:placeholder:text-zinc-600 dark:focus:border-zinc-500 dark:focus:ring-zinc-600"
                            placeholder="–"
                          />
                        </td>
                      ))}
                      <td className="px-3 py-2.5 text-right text-sm font-semibold tabular-nums text-zinc-900 dark:text-zinc-100">
                        0
                      </td>
                    </tr>
                  ))
                )}
              </tbody>
            </table>
          </div>
        </FullScreenWrapper>
      </motion.div>
    </motion.div>
  );
}
