"use client";

import { useState, useEffect } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { exportPartyStatement } from "@/lib/export-pdf";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Badge } from "@/components/ui/badge";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { FileText, Download, Send } from "lucide-react";
import { springs } from "@/lib/design-tokens";

interface StatementData {
  party: string;
  openingBalance: number;
  closingBalance: number;
  totalDebit: number;
  totalCredit: number;
  entryCount: number;
  entries: { date: string; billNo: string | null; txnType: string; debit: number; credit: number; balance: number }[];
  dateRange: { from: string; to: string };
}

interface PartyOption { name: string; normalizedName: string; }
const itemVariants = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: springs.default } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }

export default function StatementPage() {
  const { companyId } = useApp();
  const [parties, setParties] = useState<PartyOption[]>([]);
  const [party, setParty] = useState("");
  const [start, setStart] = useState("");
  const [end, setEnd] = useState("");
  const [data, setData] = useState<StatementData | null>(null);
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    fetch(`/api/parties?companyId=${companyId}`).then(r => r.json()).then(d => setParties(d.parties || [])).catch(() => {});
  }, [companyId]);

  const generate = async () => {
    if (!party.trim()) { toast.error("Select a party"); return; }
    setLoading(true);
    try {
      const params = new URLSearchParams({ party: party.trim(), companyId });
      if (start) params.set("start", start);
      if (end) params.set("end", end);
      const res = await fetch(`/api/reports/statement?${params}`);
      const result = await res.json();
      if (res.ok) {
        setData(result);
        if (result.entryCount === 0) toast.info("No transactions in this range");
      } else {
        toast.error(result.error || "Failed");
      }
    } catch { toast.error("Network error"); }
    setLoading(false);
  };

  const downloadPdf = () => {
    if (!data) return;
    exportPartyStatement({
      partyName: data.party,
      openingBalance: data.openingBalance,
      entries: data.entries,
      closingBalance: data.closingBalance,
      dateRange: data.dateRange,
    });
    toast.success("Statement PDF downloaded");
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div variants={itemVariants}>
        <h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-50">Party Statement</h1>
        <p className="mt-1 text-sm text-zinc-600 dark:text-zinc-300">Generate account statement to share with customers/suppliers</p>
      </motion.div>

      {/* Filters */}
      <motion.div variants={itemVariants}>
        <Card className="p-4">
          <div className="flex flex-wrap items-end gap-4">
            <div className="flex-1 min-w-[200px]">
              <label className="mb-1.5 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Party</label>
              <Input value={party} onChange={e => setParty(e.target.value)} placeholder="Select party..." list="stmt-parties" />
              <datalist id="stmt-parties">{parties.map(p => <option key={p.normalizedName} value={p.name} />)}</datalist>
            </div>
            <div>
              <label className="mb-1.5 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">From</label>
              <Input type="date" value={start} onChange={e => setStart(e.target.value)} className="w-40" />
            </div>
            <div>
              <label className="mb-1.5 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">To</label>
              <Input type="date" value={end} onChange={e => setEnd(e.target.value)} className="w-40" />
            </div>
            <Button onClick={generate} disabled={loading}>{loading ? "Generating..." : "Generate"}</Button>
          </div>
        </Card>
      </motion.div>

      {/* Statement Preview */}
      {data && (
        <motion.div variants={itemVariants} className="space-y-4">
          {/* Header */}
          <Card className="p-4">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-lg font-semibold text-zinc-900 dark:text-zinc-50">Statement: {data.party}</p>
                <p className="text-sm text-zinc-600 dark:text-zinc-300">
                  {data.dateRange.from && data.dateRange.to ? `${data.dateRange.from} to ${data.dateRange.to}` : "All time"} • {data.entryCount} entries
                </p>
              </div>
              <div className="flex gap-2">
                <Button variant="outline" size="sm" onClick={downloadPdf}>
                  <FileText className="mr-1.5 h-3.5 w-3.5" /> Download PDF
                </Button>
              </div>
            </div>

            {/* Balance Summary */}
            <div className="mt-4 grid grid-cols-2 gap-3 sm:grid-cols-4">
              <div className="rounded-lg bg-zinc-50 dark:bg-zinc-800/50 p-3 text-center">
                <p className="text-[11px] font-medium text-zinc-500">Opening</p>
                <p className={`mt-0.5 text-base font-bold tabular-nums ${data.openingBalance >= 0 ? "text-red-600" : "text-emerald-600"}`}>
                  ₹{fmt(Math.abs(data.openingBalance))} {data.openingBalance >= 0 ? "Dr" : "Cr"}
                </p>
              </div>
              <div className="rounded-lg bg-zinc-50 dark:bg-zinc-800/50 p-3 text-center">
                <p className="text-[11px] font-medium text-zinc-500">Total Debit</p>
                <p className="mt-0.5 text-base font-bold tabular-nums text-red-600">₹{fmt(data.totalDebit)}</p>
              </div>
              <div className="rounded-lg bg-zinc-50 dark:bg-zinc-800/50 p-3 text-center">
                <p className="text-[11px] font-medium text-zinc-500">Total Credit</p>
                <p className="mt-0.5 text-base font-bold tabular-nums text-emerald-600">₹{fmt(data.totalCredit)}</p>
              </div>
              <div className="rounded-lg bg-zinc-50 dark:bg-zinc-800/50 p-3 text-center">
                <p className="text-[11px] font-medium text-zinc-500">Closing</p>
                <p className={`mt-0.5 text-base font-bold tabular-nums ${data.closingBalance >= 0 ? "text-red-600" : "text-emerald-600"}`}>
                  ₹{fmt(Math.abs(data.closingBalance))} {data.closingBalance >= 0 ? "Dr" : "Cr"}
                </p>
              </div>
            </div>
          </Card>

          {/* Entries Table */}
          <div className="overflow-auto rounded-xl border border-zinc-300 dark:border-zinc-700 bg-white dark:bg-zinc-900/60 max-h-[60vh]">
            <table className="w-full border-collapse text-sm">
              <thead className="sticky top-0 z-10 bg-zinc-100 dark:bg-zinc-800">
                <tr>
                  <th className="border-b border-zinc-300 dark:border-zinc-700 px-3 py-2.5 text-left text-[10px] font-semibold uppercase tracking-wider text-zinc-500">#</th>
                  <th className="border-b border-zinc-300 dark:border-zinc-700 px-3 py-2.5 text-left text-[10px] font-semibold uppercase tracking-wider text-zinc-500">Date</th>
                  <th className="border-b border-zinc-300 dark:border-zinc-700 px-3 py-2.5 text-left text-[10px] font-semibold uppercase tracking-wider text-zinc-500">Particulars</th>
                  <th className="border-b border-zinc-300 dark:border-zinc-700 px-3 py-2.5 text-right text-[10px] font-semibold uppercase tracking-wider text-red-500">Debit</th>
                  <th className="border-b border-zinc-300 dark:border-zinc-700 px-3 py-2.5 text-right text-[10px] font-semibold uppercase tracking-wider text-emerald-600">Credit</th>
                  <th className="border-b border-zinc-300 dark:border-zinc-700 px-3 py-2.5 text-right text-[10px] font-semibold uppercase tracking-wider text-zinc-500">Balance</th>
                </tr>
              </thead>
              <tbody>
                {data.openingBalance !== 0 && (
                  <tr className="bg-zinc-50/50 dark:bg-zinc-800/30">
                    <td className="px-3 py-2 text-xs text-zinc-400"></td>
                    <td className="px-3 py-2 text-xs text-zinc-500"></td>
                    <td className="px-3 py-2 text-xs font-medium text-zinc-700 dark:text-zinc-300 italic">Opening Balance (B/F)</td>
                    <td className="px-3 py-2"></td>
                    <td className="px-3 py-2"></td>
                    <td className="px-3 py-2 text-right font-mono text-xs font-semibold">
                      ₹{fmt(Math.abs(data.openingBalance))} {data.openingBalance >= 0 ? "Dr" : "Cr"}
                    </td>
                  </tr>
                )}
                {data.entries.map((e, i) => (
                  <tr key={i} className="border-t border-zinc-100 dark:border-zinc-800 hover:bg-zinc-50/50 dark:hover:bg-zinc-800/30">
                    <td className="px-3 py-2 text-xs text-zinc-400 tabular-nums">{i + 1}</td>
                    <td className="px-3 py-2 text-xs tabular-nums">{new Date(e.date).toLocaleDateString("en-IN")}</td>
                    <td className="px-3 py-2 text-xs">
                      <span className="font-medium">{e.txnType}</span>
                      {e.billNo && <span className="text-zinc-400 ml-1.5">#{e.billNo}</span>}
                    </td>
                    <td className="px-3 py-2 text-right font-mono text-xs text-red-600">{e.debit > 0 ? `₹${fmt(e.debit)}` : ""}</td>
                    <td className="px-3 py-2 text-right font-mono text-xs text-emerald-600">{e.credit > 0 ? `₹${fmt(e.credit)}` : ""}</td>
                    <td className="px-3 py-2 text-right font-mono text-xs font-semibold">₹{fmt(Math.abs(e.balance))} {e.balance >= 0 ? "Dr" : "Cr"}</td>
                  </tr>
                ))}
                {/* Closing balance row */}
                <tr className="border-t-2 border-zinc-300 dark:border-zinc-600 bg-zinc-100/60 dark:bg-zinc-800/50 font-bold">
                  <td colSpan={3} className="px-3 py-3 text-xs font-semibold">CLOSING BALANCE</td>
                  <td className="px-3 py-3 text-right font-mono text-xs font-bold text-red-700">₹{fmt(data.totalDebit)}</td>
                  <td className="px-3 py-3 text-right font-mono text-xs font-bold text-emerald-700">₹{fmt(data.totalCredit)}</td>
                  <td className="px-3 py-3 text-right font-mono text-xs font-bold">
                    ₹{fmt(Math.abs(data.closingBalance))} {data.closingBalance >= 0 ? "Dr" : "Cr"}
                  </td>
                </tr>
              </tbody>
            </table>
          </div>
        </motion.div>
      )}
    </motion.div>
  );
}
