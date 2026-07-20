"use client";

import { useState } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Badge } from "@/components/ui/badge";
import { ArrowUpRight, ArrowDownRight, Minus } from "lucide-react";
import { springs } from "@/lib/design-tokens";

interface Period { start: string; end: string; sales: number; expenses: number; receipts: number; transactions: number; profit: number; }
interface Comparison { period1: Period; period2: Period; change: { sales: number; expenses: number; profit: number }; }
const itemVariants = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: springs.default } };
function fmt(n: number) { return `₹${n.toLocaleString("en-IN", { minimumFractionDigits: 0 })}`; }

function ChangeIndicator({ value }: { value: number }) {
  if (value > 0) return <span className="inline-flex items-center gap-0.5 text-emerald-600 text-xs font-medium"><ArrowUpRight className="h-3 w-3"/>+{value}%</span>;
  if (value < 0) return <span className="inline-flex items-center gap-0.5 text-red-500 text-xs font-medium"><ArrowDownRight className="h-3 w-3"/>{value}%</span>;
  return <span className="inline-flex items-center gap-0.5 text-zinc-400 text-xs"><Minus className="h-3 w-3"/>0%</span>;
}

export default function ComparisonPage() {
  const { companyId } = useApp();
  const [p1Start, setP1Start] = useState("");
  const [p1End, setP1End] = useState("");
  const [p2Start, setP2Start] = useState("");
  const [p2End, setP2End] = useState("");
  const [data, setData] = useState<Comparison | null>(null);
  const [loading, setLoading] = useState(false);

  const load = async () => {
    if (!p1Start || !p1End || !p2Start || !p2End) { toast.error("All 4 dates required"); return; }
    setLoading(true);
    try {
      const params = new URLSearchParams({ companyId, p1Start, p1End, p2Start, p2End });
      const res = await fetch(`/api/reports/comparison?${params}`);
      const d = await res.json();
      if (res.ok) setData(d); else toast.error(d.error || "Failed");
    } catch { toast.error("Network error"); }
    setLoading(false);
  };

  const setPreset = (type: "month" | "year") => {
    const now = new Date();
    if (type === "month") {
      const thisStart = new Date(now.getFullYear(), now.getMonth(), 1);
      const lastStart = new Date(now.getFullYear(), now.getMonth() - 1, 1);
      const lastEnd = new Date(now.getFullYear(), now.getMonth(), 0);
      setP1Start(lastStart.toISOString().split("T")[0]); setP1End(lastEnd.toISOString().split("T")[0]);
      setP2Start(thisStart.toISOString().split("T")[0]); setP2End(now.toISOString().split("T")[0]);
    } else {
      const thisYearStart = new Date(now.getFullYear(), 0, 1);
      const lastYearStart = new Date(now.getFullYear() - 1, 0, 1);
      const lastYearEnd = new Date(now.getFullYear() - 1, 11, 31);
      setP1Start(lastYearStart.toISOString().split("T")[0]); setP1End(lastYearEnd.toISOString().split("T")[0]);
      setP2Start(thisYearStart.toISOString().split("T")[0]); setP2End(now.toISOString().split("T")[0]);
    }
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5 max-w-4xl">
      <motion.div variants={itemVariants}><h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-100">Period Comparison</h1><p className="mt-0.5 text-sm text-zinc-500">Compare two time periods side by side</p></motion.div>

      <motion.div variants={itemVariants}><Card className="p-4">
        <div className="grid grid-cols-1 gap-4 sm:grid-cols-2">
          <div><p className="text-xs font-semibold text-zinc-500 mb-2">Period 1 (Previous)</p><div className="flex gap-2"><Input type="date" value={p1Start} onChange={e=>setP1Start(e.target.value)} className="flex-1"/><Input type="date" value={p1End} onChange={e=>setP1End(e.target.value)} className="flex-1"/></div></div>
          <div><p className="text-xs font-semibold text-zinc-500 mb-2">Period 2 (Current)</p><div className="flex gap-2"><Input type="date" value={p2Start} onChange={e=>setP2Start(e.target.value)} className="flex-1"/><Input type="date" value={p2End} onChange={e=>setP2End(e.target.value)} className="flex-1"/></div></div>
        </div>
        <div className="flex gap-2 mt-3"><Button variant="secondary" size="sm" onClick={()=>setPreset("month")}>This Month vs Last</Button><Button variant="secondary" size="sm" onClick={()=>setPreset("year")}>This Year vs Last</Button><Button size="sm" onClick={load} disabled={loading}>{loading?"...":"Compare"}</Button></div>
      </Card></motion.div>

      {data && (
        <motion.div variants={itemVariants}><div className="grid grid-cols-1 gap-4 sm:grid-cols-3">
          <Card className="p-4 text-center"><p className="text-[11px] uppercase tracking-wider text-zinc-500">Sales</p><p className="text-lg font-semibold text-zinc-900 dark:text-zinc-100 mt-1">{fmt(data.period2.sales)}</p><p className="text-xs text-zinc-400">was {fmt(data.period1.sales)}</p><div className="mt-2"><ChangeIndicator value={data.change.sales}/></div></Card>
          <Card className="p-4 text-center"><p className="text-[11px] uppercase tracking-wider text-zinc-500">Expenses</p><p className="text-lg font-semibold text-zinc-900 dark:text-zinc-100 mt-1">{fmt(data.period2.expenses)}</p><p className="text-xs text-zinc-400">was {fmt(data.period1.expenses)}</p><div className="mt-2"><ChangeIndicator value={data.change.expenses}/></div></Card>
          <Card className="p-4 text-center"><p className="text-[11px] uppercase tracking-wider text-zinc-500">Net Profit</p><p className={`text-lg font-semibold mt-1 ${data.period2.profit>=0?"text-emerald-600":"text-red-500"}`}>{fmt(data.period2.profit)}</p><p className="text-xs text-zinc-400">was {fmt(data.period1.profit)}</p><div className="mt-2"><ChangeIndicator value={data.change.profit}/></div></Card>
        </div></motion.div>
      )}
    </motion.div>
  );
}
