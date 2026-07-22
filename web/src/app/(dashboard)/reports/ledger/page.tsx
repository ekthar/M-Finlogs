"use client";

import { useState, useEffect } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Badge } from "@/components/ui/badge";
import { TableSkeleton } from "@/components/ui/skeleton";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { Printer, Search, FileText, Download } from "lucide-react";

interface LedgerEntry { date: string; billNo: string | null; txnType: string; paymentMode: string; debit: number; credit: number; balance: number; }
interface PartyOption { name: string; normalizedName: string; }

const fadeUp = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }

export default function LedgerPage() {
  const [parties, setParties] = useState<PartyOption[]>([]);
  const [party, setParty] = useState("");
  const [start, setStart] = useState("");
  const [end, setEnd] = useState("");
  const [ledger, setLedger] = useState<LedgerEntry[]>([]);
  const [loading, setLoading] = useState(false);
  const [partyName, setPartyName] = useState("");
  const [totalBalance, setTotalBalance] = useState(0);
  const [pdfLoading, setPdfLoading] = useState(false);
  const [excelLoading, setExcelLoading] = useState(false);

  // Load party list on mount
  useEffect(() => {
    fetch("/api/parties")
      .then((r) => r.json())
      .then((d) => setParties(d.parties || []))
      .catch(() => {});
  }, []);

  const load = async () => {
    if (!party.trim()) {
      toast.error("Please select or enter a party name");
      return;
    }
    setLoading(true);
    try {
      const params = new URLSearchParams({ party: party.trim() });
      if (start) params.set("start", start);
      if (end) params.set("end", end);
      const res = await fetch(`/api/reports/ledger?${params}`);
      const data = await res.json();
      if (res.ok) {
        setLedger(data.ledger || []);
        setPartyName(data.party || party);
        setTotalBalance(data.totalBalance || 0);
        if ((data.ledger || []).length === 0) toast.info("No transactions found for this party");
      } else {
        toast.error(data.error || "Failed to load ledger");
        setLedger([]);
      }
    } catch { toast.error("Network error"); }
    setLoading(false);
  };

  const handlePdfExport = async () => {
    if (ledger.length === 0) { toast.error("No data to export"); return; }
    setPdfLoading(true);
    try {
      const { exportLedgerPdf } = await import("@/lib/export-pdf");
      await exportLedgerPdf({ title: "Party Ledger", partyName, balance: totalBalance, entries: ledger.map((e) => ({ ...e, billNo: e.billNo || "" })) });
      toast.success("PDF downloaded");
    } catch { toast.error("PDF export failed"); }
    setPdfLoading(false);
  };

  const handleExcelExport = async () => {
    if (ledger.length === 0) { toast.error("No data to export"); return; }
    setExcelLoading(true);
    try {
      const { exportTableExcel } = await import("@/lib/export-excel");
      await exportTableExcel({
        sheetName: "Ledger",
        headers: ["Date", "Bill No", "Type", "Mode", "Debit", "Credit", "Balance"],
        rows: ledger.map((e) => [new Date(e.date).toLocaleDateString("en-IN"), e.billNo || "—", e.txnType, e.paymentMode, e.debit, e.credit, e.balance]),
        filename: `ledger_${partyName.replace(/\s+/g, "_").toLowerCase()}.xlsx`,
      });
      toast.success("Excel downloaded");
    } catch { toast.error("Excel export failed"); }
    setExcelLoading(false);
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-2">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-50">Party Ledger</h1>
          <p className="mt-1 text-sm text-zinc-600 dark:text-zinc-300">{partyName ? `Showing: ${partyName} • Balance: ₹${fmt(totalBalance)}` : "Select a party to view statement"}</p>
        </div>
        <div className="flex items-center gap-2">
          <Button variant="outline" size="sm" onClick={handlePdfExport} disabled={pdfLoading}>
            <FileText className="mr-1.5 h-3.5 w-3.5" /> {pdfLoading ? "..." : "PDF"}
          </Button>
          <Button variant="outline" size="sm" onClick={handleExcelExport} disabled={excelLoading}>
            <Download className="mr-1.5 h-3.5 w-3.5" /> {excelLoading ? "..." : "Excel"}
          </Button>
          <Button variant="outline" size="sm" onClick={() => window.print()}>
            <Printer className="mr-1.5 h-3.5 w-3.5" /> Print
          </Button>
        </div>
      </motion.div>

      {/* Filters with party datalist */}
      <motion.div {...fadeUp}>
        <Card className="p-4">
          <div className="flex flex-wrap items-end gap-4">
            <div className="flex-1 min-w-[200px]">
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Party</label>
              <Input
                placeholder="Type to search parties..."
                value={party}
                onChange={(e) => setParty(e.target.value)}
                list="ledger-party-list"
                onKeyDown={(e) => { if (e.key === "Enter") load(); }}
              />
              <datalist id="ledger-party-list">
                {parties.map((p) => (
                  <option key={p.normalizedName} value={p.name} />
                ))}
              </datalist>
            </div>
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">From</label>
              <Input type="date" value={start} onChange={(e) => setStart(e.target.value)} className="w-40" />
            </div>
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">To</label>
              <Input type="date" value={end} onChange={(e) => setEnd(e.target.value)} className="w-40" />
            </div>
            <Button size="sm" onClick={load} disabled={loading}>{loading ? "Loading..." : "Show Ledger"}</Button>
          </div>
        </Card>
      </motion.div>

      {/* Balance chip */}
      {partyName && ledger.length > 0 && (
        <motion.div {...fadeUp}>
          <div className="inline-flex items-center gap-2 rounded-xl bg-zinc-100 px-4 py-2 dark:bg-zinc-800">
            <span className="text-xs font-medium text-zinc-600 dark:text-zinc-300">Closing Balance:</span>
            <span className={`text-sm font-semibold ${totalBalance >= 0 ? "text-red-500" : "text-emerald-600"}`}>
              ₹{fmt(Math.abs(totalBalance))} {totalBalance >= 0 ? "Dr" : "Cr"}
            </span>
          </div>
        </motion.div>
      )}

      {/* Table */}
      <motion.div {...fadeUp}>
        {loading ? <TableSkeleton rows={8} cols={7} /> : (
          <Table>
            <TableHeader>
              <TableRow>
                <TableHead>Date</TableHead>
                <TableHead className="hidden sm:table-cell">Bill</TableHead>
                <TableHead>Type</TableHead>
                <TableHead className="hidden md:table-cell">Mode</TableHead>
                <TableHead className="text-right">Debit</TableHead>
                <TableHead className="text-right">Credit</TableHead>
                <TableHead className="text-right">Balance</TableHead>
              </TableRow>
            </TableHeader>
            <TableBody>
              {ledger.length === 0 ? (
                <TableRow>
                  <TableCell colSpan={7} className="h-32 text-center">
                    <div className="flex flex-col items-center gap-2">
                      <div className="text-3xl">📖</div>
                      <p className="text-sm text-zinc-600 dark:text-zinc-300">Select a party and click "Show Ledger"</p>
                      <p className="text-sm text-zinc-500 dark:text-zinc-400 mt-0.5">Start typing in the Party field to see suggestions</p>
                    </div>
                  </TableCell>
                </TableRow>
              ) : ledger.map((e, i) => (
                <TableRow key={i}>
                  <TableCell className="text-xs">{new Date(e.date).toLocaleDateString("en-IN")}</TableCell>
                  <TableCell className="hidden sm:table-cell">{e.billNo || "—"}</TableCell>
                  <TableCell><Badge variant={e.txnType === "Sale" ? "success" : e.txnType === "Expense" ? "danger" : e.txnType === "Receipt" ? "info" : "default"}>{e.txnType}</Badge></TableCell>
                  <TableCell className="hidden md:table-cell">{e.paymentMode}</TableCell>
                  <TableCell className="text-right font-mono text-red-600">{e.debit > 0 ? `₹${fmt(e.debit)}` : ""}</TableCell>
                  <TableCell className="text-right font-mono text-emerald-600">{e.credit > 0 ? `₹${fmt(e.credit)}` : ""}</TableCell>
                  <TableCell className="text-right font-mono font-medium">{fmt(e.balance)}</TableCell>
                </TableRow>
              ))}
              {ledger.length > 0 && (
                <TableRow className="border-t-2 border-zinc-200 dark:border-zinc-700 font-semibold">
                  <TableCell colSpan={4} className="hidden md:table-cell">Total</TableCell>
                  <TableCell colSpan={2} className="md:hidden">Total</TableCell>
                  <TableCell className="text-right font-mono text-red-600">₹{fmt(ledger.reduce((s, e) => s + e.debit, 0))}</TableCell>
                  <TableCell className="text-right font-mono text-emerald-600">₹{fmt(ledger.reduce((s, e) => s + e.credit, 0))}</TableCell>
                  <TableCell className="text-right font-mono font-bold">₹{fmt(totalBalance)}</TableCell>
                </TableRow>
              )}
            </TableBody>
          </Table>
        )}
      </motion.div>
    </motion.div>
  );
}
