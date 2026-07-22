"use client";

import { useState, useEffect, useCallback } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Select } from "@/components/ui/select";
import { Badge } from "@/components/ui/badge";
import { TableSkeleton } from "@/components/ui/skeleton";
import { FullScreenWrapper } from "@/components/full-screen-wrapper";
import { Save, Plus, Minus, Download, FileText } from "lucide-react";
import { exportToExcel } from "@/lib/export-excel";
import { exportToPdf } from "@/lib/export-pdf";
import { springs } from "@/lib/design-tokens";

interface InventoryRow {
  id: string; dbId?: number; name: string; cost: number; minStock: number;
  qty: number[]; purchaseQty: number[];
}

const MONTHS = ["January","February","March","April","May","June","July","August","September","October","November","December"];
const itemVariants = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: springs.default } };

export default function InventoryPage() {
  const { companyId, financialYear } = useApp();
  const [rows, setRows] = useState<InventoryRow[]>([]);
  const [month, setMonth] = useState(new Date().getMonth() + 1);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [newProduct, setNewProduct] = useState("");
  const [pdfLoading, setPdfLoading] = useState(false);

  const today = new Date();
  const daysInMonth = new Date(today.getFullYear(), month, 0).getDate();
  const currentDay = month === today.getMonth() + 1 ? today.getDate() : daysInMonth;

  const loadData = useCallback(async () => {
    setLoading(true);
    try {
      const res = await fetch(`/api/inventory/snapshot?companyId=${companyId}&financialYear=${financialYear}&month=${month}`);
      const data = await res.json();
      setRows(data.items || []);
    } catch {}
    setLoading(false);
  }, [companyId, financialYear, month]);

  useEffect(() => { loadData(); }, [loadData]);

  const handleSave = async () => {
    setSaving(true);
    try {
      const res = await fetch("/api/inventory/snapshot", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ companyId, financialYear, month, rows }),
      });
      if (res.ok) toast.success("Inventory saved");
      else toast.error("Failed to save");
    } catch { toast.error("Network error"); }
    setSaving(false);
  };

  const addProduct = () => {
    if (!newProduct.trim()) return;
    const id = `inv-${Date.now()}-${Math.random().toString(36).slice(2,6)}`;
    setRows(prev => [...prev, { id, name: newProduct.trim(), cost: 0, minStock: 0, qty: Array(31).fill(0), purchaseQty: Array(31).fill(0) }]);
    setNewProduct("");
    toast.success(`Added: ${newProduct.trim()}`);
  };

  const updateQty = (rowIdx: number, day: number, value: number) => {
    setRows(prev => prev.map((r, i) => {
      if (i !== rowIdx) return r;
      const qty = [...r.qty];
      qty[day] = value;
      return { ...r, qty };
    }));
  };

  const removeEmpty = () => {
    const before = rows.length;
    setRows(prev => prev.filter(r => r.name.trim() !== "" || r.qty.some(q => q > 0)));
    toast.success(`Removed ${before - rows.length} empty rows`);
  };

  const totalQty = rows.reduce((s, r) => s + (r.qty[currentDay - 1] || 0), 0);
  const totalPurchase = rows.reduce((s, r) => s + r.purchaseQty.reduce((a, b) => a + b, 0), 0);
  const reorderCount = rows.filter(r => r.minStock > 0 && (r.qty[currentDay - 1] || 0) < r.minStock).length;

  const handleExcelExport = () => {
    if (!rows.length) return;
    const data = rows.map(r => ({ Product: r.name, Cost: r.cost, MinStock: r.minStock, CurrentQty: r.qty[currentDay-1]||0 }));
    exportToExcel(data, `Inventory_${MONTHS[month-1]}_${financialYear}`);
    toast.success("Excel exported");
  };

  const handlePdfExport = () => {
    if (!rows.length) return;
    setPdfLoading(true);
    try {
      const headers = ["Product", "Cost (₹)", "Min Stock", `Qty (Day ${currentDay})`, "Status"];
      const pdfRows = rows.map(r => {
        const qty = r.qty[currentDay-1] || 0;
        const status = r.minStock > 0 && qty < r.minStock ? "LOW" : "OK";
        return [r.name, String(r.cost), String(r.minStock), String(qty), status];
      });
      exportToPdf({
        title: `Inventory Report — ${MONTHS[month-1]} ${financialYear}`,
        subtitle: `${rows.length} products | Day ${currentDay} snapshot`,
        headers,
        rows: pdfRows,
        filename: `Inventory_${MONTHS[month-1]}_${financialYear}`,
        orientation: "portrait",
      });
      toast.success("PDF exported");
    } catch { toast.error("PDF export failed"); }
    setPdfLoading(false);
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-4">
      <motion.div variants={itemVariants} className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-2">
        <div>
          <h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-50">Inventory Management</h1>
          <p className="mt-1 text-sm text-zinc-600 dark:text-zinc-300">{MONTHS[month-1]} {financialYear} • {rows.length} products</p>
        </div>
        <div className="flex flex-wrap items-center gap-2">
          <Button variant="outline" size="sm" onClick={handlePdfExport} disabled={pdfLoading || !rows.length}><FileText className="mr-1.5 h-3.5 w-3.5"/>{pdfLoading ? "..." : "PDF"}</Button>
          <Button variant="outline" size="sm" onClick={handleExcelExport} disabled={!rows.length}><Download className="mr-1.5 h-3.5 w-3.5"/>Excel</Button>
          <Button size="sm" onClick={handleSave} disabled={saving}><Save className="mr-1.5 h-3.5 w-3.5"/>{saving ? "Saving..." : "Save"}</Button>
        </div>
      </motion.div>

      {/* Toolbar */}
      <motion.div variants={itemVariants}><Card className="p-3">
        <div className="flex flex-wrap items-end gap-3">
          <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Month</label>
            <Select value={String(month)} onChange={e => setMonth(parseInt(e.target.value))} className="w-40">
              {MONTHS.map((m, i) => <option key={i} value={i+1}>{m}</option>)}
            </Select></div>
          <div className="flex items-end gap-2">
            <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Add Product</label><Input value={newProduct} onChange={e => setNewProduct(e.target.value)} placeholder="Product name" className="w-52" onKeyDown={e => { if (e.key === "Enter") addProduct(); }}/></div>
            <Button variant="secondary" size="sm" onClick={addProduct}><Plus className="mr-1 h-3.5 w-3.5"/>Add</Button>
            <Button variant="ghost" size="sm" onClick={removeEmpty}><Minus className="mr-1 h-3.5 w-3.5"/>Clean</Button>
          </div>
        </div>
      </Card></motion.div>

      {/* Summary */}
      <motion.div variants={itemVariants}><Card className="p-3">
        <div className="grid grid-cols-2 gap-4 sm:grid-cols-4">
          <div><p className="text-[11px] uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Current Qty</p><p className="mt-0.5 text-xl font-bold tabular-nums text-zinc-900 dark:text-zinc-50">{totalQty.toLocaleString("en-IN")}</p></div>
          <div><p className="text-[11px] uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Purchase In</p><p className="mt-0.5 text-xl font-bold tabular-nums text-zinc-900 dark:text-zinc-50">{totalPurchase.toLocaleString("en-IN")}</p></div>
          <div><p className="text-[11px] uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Products</p><p className="mt-0.5 text-xl font-bold tabular-nums text-zinc-900 dark:text-zinc-50">{rows.length}</p></div>
          <div><p className="text-[11px] uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Reorder Alert</p><p className="mt-0.5 text-xl font-bold tabular-nums text-red-600 dark:text-red-400">{reorderCount}</p></div>
        </div>
      </Card></motion.div>

      {/* Grid */}
      <motion.div variants={itemVariants}>
        {loading ? <TableSkeleton rows={6} cols={10}/> : (
          <FullScreenWrapper title="Inventory Management">
            <div className="overflow-x-auto rounded-2xl border border-zinc-200/50 bg-white/80 backdrop-blur-xl dark:border-zinc-700/50 dark:bg-zinc-900/80">
              <table className="w-full text-xs">
                <thead className="sticky top-0 z-10">
                  <tr className="bg-zinc-50/95 dark:bg-zinc-800/95 backdrop-blur-sm">
                    <th className="sticky left-0 z-20 bg-zinc-100 dark:bg-zinc-800 px-3 py-2.5 text-left text-[11px] font-semibold uppercase tracking-wider text-zinc-700 dark:text-zinc-200 w-[160px]">Product</th>
                    <th className="px-2 py-2.5 text-right text-[11px] font-semibold text-zinc-700 dark:text-zinc-200 w-[60px]">Cost</th>
                    <th className="px-2 py-2.5 text-right text-[11px] font-semibold text-zinc-700 dark:text-zinc-200 w-[50px]">Min</th>
                    {Array.from({length: daysInMonth}, (_, i) => i + 1).map(d => (
                      <th key={d} className={`min-w-[44px] px-1 py-2.5 text-center text-[11px] font-semibold tabular-nums ${d === currentDay ? "bg-blue-50 text-blue-700 dark:bg-blue-900/30 dark:text-blue-400" : "text-zinc-600 dark:text-zinc-300"}`}>{d}</th>
                    ))}
                    <th className="min-w-[70px] px-3 py-2.5 text-right text-[11px] font-semibold uppercase tracking-wider text-zinc-700 dark:text-zinc-200">Total</th>
                  </tr>
                </thead>
                <tbody>
                  {rows.map((row, ri) => {
                    const isReorder = row.minStock > 0 && (row.qty[currentDay-1]||0) < row.minStock;
                    const rowTotal = row.qty.slice(0, daysInMonth).reduce((a, b) => a + b, 0);
                    return (
                      <tr key={row.id} className={`border-t border-zinc-100 dark:border-zinc-800 transition-colors hover:bg-zinc-50/80 dark:hover:bg-zinc-800/50 ${isReorder ? "bg-red-50/30 dark:bg-red-900/5" : ""}`}>
                        <td className="sticky left-0 z-10 bg-white dark:bg-zinc-900 px-3 py-2 text-sm font-medium text-zinc-900 dark:text-zinc-100 truncate max-w-[160px]">
                          {row.name} {isReorder && <Badge variant="danger" className="ml-1 text-[9px]">LOW</Badge>}
                        </td>
                        <td className="px-2 py-1.5"><input type="number" className="w-full bg-transparent text-right text-xs tabular-nums text-zinc-700 dark:text-zinc-200 outline-none rounded focus:bg-blue-50 dark:focus:bg-blue-900/20 h-7" value={row.cost || ""} onChange={e => setRows(prev => prev.map((r,i) => i===ri?{...r,cost:parseFloat(e.target.value)||0}:r))}/></td>
                        <td className="px-2 py-1.5"><input type="number" className="w-full bg-transparent text-right text-xs tabular-nums text-zinc-700 dark:text-zinc-200 outline-none rounded focus:bg-blue-50 dark:focus:bg-blue-900/20 h-7" value={row.minStock || ""} onChange={e => setRows(prev => prev.map((r,i) => i===ri?{...r,minStock:parseFloat(e.target.value)||0}:r))}/></td>
                        {Array.from({length: daysInMonth}, (_, i) => i + 1).map(d => (
                          <td key={d} className={`px-0.5 py-1 ${d === currentDay ? "bg-blue-50/50 dark:bg-blue-900/10" : ""}`}>
                            <input
                              type="number"
                              className="w-full bg-transparent text-center text-xs tabular-nums text-zinc-700 dark:text-zinc-200 outline-none rounded focus:bg-blue-50 dark:focus:bg-blue-900/20 h-7"
                              value={row.qty[d-1] || ""}
                              onChange={e => updateQty(ri, d-1, parseFloat(e.target.value)||0)}
                            />
                          </td>
                        ))}
                        <td className="px-3 py-2 text-right text-sm font-semibold tabular-nums text-zinc-900 dark:text-zinc-100">{rowTotal}</td>
                      </tr>
                    );
                  })}
                  {rows.length === 0 && (
                    <tr><td colSpan={daysInMonth + 4} className="h-32 text-center text-sm text-zinc-600 dark:text-zinc-300">Add products to start tracking inventory</td></tr>
                  )}
                </tbody>
              </table>
            </div>
          </FullScreenWrapper>
        )}
      </motion.div>
    </motion.div>
  );
}
