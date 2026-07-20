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
import { Save, Plus, Minus, Download } from "lucide-react";
import { exportToExcel } from "@/lib/export-excel";
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

  const handleExport = () => {
    if (!rows.length) return;
    const data = rows.map(r => ({ Product: r.name, Cost: r.cost, MinStock: r.minStock, CurrentQty: r.qty[currentDay-1]||0 }));
    exportToExcel(data, `Inventory_${MONTHS[month-1]}_${financialYear}`);
    toast.success("Exported");
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-4">
      <motion.div variants={itemVariants} className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-2">
        <div>
          <h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-100">Inventory Management</h1>
          <p className="mt-0.5 text-sm text-zinc-500">{MONTHS[month-1]} {financialYear} • {rows.length} products</p>
        </div>
        <div className="flex flex-wrap gap-2">
          <Button variant="outline" size="sm" onClick={handleExport}><Download className="mr-1.5 h-3.5 w-3.5"/>Export</Button>
          <Button size="sm" onClick={handleSave} disabled={saving}><Save className="mr-1.5 h-3.5 w-3.5"/>{saving ? "Saving..." : "Save"}</Button>
        </div>
      </motion.div>

      {/* Toolbar */}
      <motion.div variants={itemVariants}><Card className="p-3">
        <div className="flex flex-wrap items-end gap-3">
          <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Month</label>
            <Select value={String(month)} onChange={e => setMonth(parseInt(e.target.value))} className="w-40">
              {MONTHS.map((m, i) => <option key={i} value={i+1}>{m}</option>)}
            </Select></div>
          <div className="flex items-end gap-2">
            <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Add Product</label><Input value={newProduct} onChange={e => setNewProduct(e.target.value)} placeholder="Product name" className="w-52" onKeyDown={e => { if (e.key === "Enter") addProduct(); }}/></div>
            <Button variant="secondary" size="sm" onClick={addProduct}><Plus className="mr-1 h-3.5 w-3.5"/>Add</Button>
            <Button variant="ghost" size="sm" onClick={removeEmpty}><Minus className="mr-1 h-3.5 w-3.5"/>Clean</Button>
          </div>
        </div>
      </Card></motion.div>

      {/* Summary */}
      <motion.div variants={itemVariants}><Card className="p-3">
        <div className="grid grid-cols-2 gap-4 sm:grid-cols-4">
          <div><p className="text-[11px] uppercase tracking-wider text-zinc-500">Current Qty</p><p className="mt-0.5 text-lg font-semibold">{totalQty.toLocaleString("en-IN")}</p></div>
          <div><p className="text-[11px] uppercase tracking-wider text-zinc-500">Purchase In</p><p className="mt-0.5 text-lg font-semibold">{totalPurchase.toLocaleString("en-IN")}</p></div>
          <div><p className="text-[11px] uppercase tracking-wider text-zinc-500">Products</p><p className="mt-0.5 text-lg font-semibold">{rows.length}</p></div>
          <div><p className="text-[11px] uppercase tracking-wider text-zinc-500">Reorder Alert</p><p className="mt-0.5 text-lg font-semibold text-red-500">{reorderCount}</p></div>
        </div>
      </Card></motion.div>

      {/* Grid */}
      <motion.div variants={itemVariants}>
        {loading ? <TableSkeleton rows={6} cols={10}/> : (
          <div className="overflow-x-auto rounded-2xl border border-zinc-200/50 bg-white/72 backdrop-blur-xl dark:border-zinc-700/50 dark:bg-zinc-900/72">
            <table className="w-full text-xs">
              <thead>
                <tr className="bg-zinc-50/80 dark:bg-zinc-800/80">
                  <th className="sticky left-0 z-10 bg-zinc-50 dark:bg-zinc-800 px-3 py-2 text-left font-semibold text-zinc-600 w-[160px]">Product</th>
                  <th className="px-2 py-2 text-right font-semibold text-zinc-600 w-[60px]">Cost</th>
                  <th className="px-2 py-2 text-right font-semibold text-zinc-600 w-[50px]">Min</th>
                  {Array.from({length: Math.min(daysInMonth, currentDay + 2)}, (_, i) => i + 1).slice(-10).map(d => (
                    <th key={d} className={`px-1 py-2 text-center font-semibold w-[45px] ${d === currentDay ? "bg-blue-50 text-blue-700 dark:bg-blue-900/30 dark:text-blue-400" : "text-zinc-500"}`}>{d}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {rows.map((row, ri) => {
                  const isReorder = row.minStock > 0 && (row.qty[currentDay-1]||0) < row.minStock;
                  return (
                    <tr key={row.id} className={`border-t border-zinc-100 dark:border-zinc-800 ${isReorder ? "bg-red-50/30 dark:bg-red-900/5" : ""}`}>
                      <td className="sticky left-0 z-10 bg-white dark:bg-zinc-900 px-3 py-1.5 font-medium truncate max-w-[160px]">
                        {row.name} {isReorder && <Badge variant="danger" className="ml-1 text-[9px]">LOW</Badge>}
                      </td>
                      <td className="px-2 py-1.5"><input type="number" className="w-full bg-transparent text-right text-xs outline-none" value={row.cost || ""} onChange={e => setRows(prev => prev.map((r,i) => i===ri?{...r,cost:parseFloat(e.target.value)||0}:r))}/></td>
                      <td className="px-2 py-1.5"><input type="number" className="w-full bg-transparent text-right text-xs outline-none" value={row.minStock || ""} onChange={e => setRows(prev => prev.map((r,i) => i===ri?{...r,minStock:parseFloat(e.target.value)||0}:r))}/></td>
                      {Array.from({length: Math.min(daysInMonth, currentDay + 2)}, (_, i) => i + 1).slice(-10).map(d => (
                        <td key={d} className={`px-0.5 py-1 ${d === currentDay ? "bg-blue-50/50 dark:bg-blue-900/10" : ""}`}>
                          <input
                            type="number"
                            className="w-full bg-transparent text-center text-xs outline-none rounded focus:bg-blue-50 dark:focus:bg-blue-900/20 h-6"
                            value={row.qty[d-1] || ""}
                            onChange={e => updateQty(ri, d-1, parseFloat(e.target.value)||0)}
                          />
                        </td>
                      ))}
                    </tr>
                  );
                })}
                {rows.length === 0 && (
                  <tr><td colSpan={13} className="h-32 text-center text-sm text-zinc-500">Add products to start tracking inventory</td></tr>
                )}
              </tbody>
            </table>
          </div>
        )}
      </motion.div>
    </motion.div>
  );
}
