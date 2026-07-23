"use client";

import { useState, useEffect, useRef, useMemo } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Badge } from "@/components/ui/badge";
import { TableSkeleton } from "@/components/ui/skeleton";
import { EditTransactionModal } from "@/components/edit-transaction-modal";
import { ConfirmDialog } from "@/components/ui/confirm-dialog";
import { exportLedgerPdf } from "@/lib/export-pdf";
import { exportToExcel } from "@/lib/export-excel";
import {
  Printer, Pencil, Trash2, FileText, Search, ChevronDown, User,
  Download, ArrowUp, Hash, Calendar,
} from "lucide-react";

interface LedgerEntry {
  txnId: number;
  date: string;
  billNo: string | null;
  txnType: string;
  paymentMode: string;
  debit: number;
  credit: number;
  balance: number;
}

interface PartyOption {
  name: string;
  normalizedName: string;
}

const fadeUp = {
  initial: { opacity: 0, y: 12 },
  animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } },
};

function fmt(n: number) {
  return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Party Search Dropdown
 * ───────────────────────────────────────────────────────────────────────────── */

function PartySearchDropdown({
  parties, value, onChange, onSubmit,
}: {
  parties: PartyOption[]; value: string; onChange: (v: string) => void; onSubmit: () => void;
}) {
  const [open, setOpen] = useState(false);
  const [search, setSearch] = useState(value);
  const ref = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    const handler = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) setOpen(false);
    };
    document.addEventListener("mousedown", handler);
    return () => document.removeEventListener("mousedown", handler);
  }, []);

  useEffect(() => { setSearch(value); }, [value]);

  const filtered = parties.filter((p) => p.name.toLowerCase().includes(search.toLowerCase()));

  return (
    <div ref={ref} className="relative">
      <div
        className="flex items-center gap-2 rounded-lg border border-zinc-200 dark:border-zinc-700 bg-white dark:bg-zinc-900 px-3 py-2 cursor-text transition-all focus-within:ring-2 focus-within:ring-zinc-900/10 dark:focus-within:ring-zinc-100/10 focus-within:border-zinc-300 dark:focus-within:border-zinc-600"
        onClick={() => { setOpen(true); inputRef.current?.focus(); }}
      >
        <Search className="h-4 w-4 text-zinc-400 flex-shrink-0" />
        <input
          ref={inputRef} type="text" placeholder="Search parties..."
          value={search}
          onChange={(e) => { setSearch(e.target.value); onChange(e.target.value); setOpen(true); }}
          onFocus={() => setOpen(true)}
          onKeyDown={(e) => {
            if (e.key === "Enter") { setOpen(false); onSubmit(); }
            if (e.key === "Escape") setOpen(false);
          }}
          className="flex-1 bg-transparent text-sm text-zinc-900 dark:text-zinc-100 outline-none placeholder:text-zinc-400"
        />
        <ChevronDown className={`h-4 w-4 text-zinc-400 transition-transform ${open ? "rotate-180" : ""}`} />
      </div>
      <AnimatePresence>
        {open && (
          <motion.div
            initial={{ opacity: 0, y: 4, scale: 0.98 }} animate={{ opacity: 1, y: 0, scale: 1 }}
            exit={{ opacity: 0, y: 4, scale: 0.98 }} transition={{ type: "spring", bounce: 0, duration: 0.2 }}
            className="absolute z-50 mt-1 w-full max-h-[280px] overflow-y-auto rounded-xl border border-zinc-200 dark:border-zinc-700 bg-white dark:bg-zinc-900 shadow-xl"
          >
            <div className="p-1">
              {filtered.length === 0 ? (
                <div className="px-3 py-4 text-center text-sm text-zinc-500">No parties found</div>
              ) : filtered.map((p) => (
                <button key={p.normalizedName} onClick={() => { setSearch(p.name); onChange(p.name); setOpen(false); }}
                  className="flex w-full items-center gap-3 rounded-lg px-3 py-2.5 text-left hover:bg-zinc-50 dark:hover:bg-zinc-800 transition-colors group">
                  <div className="flex h-8 w-8 items-center justify-center rounded-full bg-gradient-to-br from-blue-100 to-indigo-100 dark:from-blue-900/30 dark:to-indigo-900/30">
                    <User className="h-3.5 w-3.5 text-blue-600 dark:text-blue-400" />
                  </div>
                  <span className="text-sm font-medium text-zinc-800 dark:text-zinc-200 truncate">{p.name}</span>
                </button>
              ))}
            </div>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Ledger Page — Production Grade
 * ───────────────────────────────────────────────────────────────────────────── */

export default function LedgerPage() {
  const { companyId } = useApp();
  const [parties, setParties] = useState<PartyOption[]>([]);
  const [party, setParty] = useState("");
  const [start, setStart] = useState("");
  const [end, setEnd] = useState("");
  const [ledger, setLedger] = useState<LedgerEntry[]>([]);
  const [loading, setLoading] = useState(false);
  const [partyName, setPartyName] = useState("");
  const [totalBalance, setTotalBalance] = useState(0);
  const [openingBalance, setOpeningBalance] = useState(0);
  const [totalCount, setTotalCount] = useState(0);
  const [editTxn, setEditTxn] = useState<any>(null);
  const [deleteId, setDeleteId] = useState<number | null>(null);
  const [showScrollTop, setShowScrollTop] = useState(false);
  const tableContainerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    fetch(`/api/parties?companyId=${companyId}`).then((r) => r.json()).then((d) => setParties(d.parties || [])).catch(() => {});
  }, [companyId]);

  // Scroll-to-top detection
  useEffect(() => {
    const container = tableContainerRef.current;
    if (!container) return;
    const handler = () => setShowScrollTop(container.scrollTop > 300);
    container.addEventListener("scroll", handler);
    return () => container.removeEventListener("scroll", handler);
  }, [ledger]);

  const load = async () => {
    if (!party.trim()) { toast.error("Please select or enter a party name"); return; }
    setLoading(true);
    try {
      const params = new URLSearchParams({ party: party.trim(), companyId });
      if (start) params.set("start", start);
      if (end) params.set("end", end);
      const res = await fetch(`/api/reports/ledger?${params}`);
      const data = await res.json();
      if (res.ok) {
        setLedger(data.ledger || []);
        setPartyName(data.party || party);
        setTotalBalance(data.totalBalance || 0);
        setOpeningBalance(data.openingBalance || 0);
        setTotalCount(data.totalCount || 0);
        if ((data.ledger || []).length === 0) toast.info("No transactions found");
      } else {
        toast.error(data.error || "Failed to load ledger");
        setLedger([]);
      }
    } catch { toast.error("Network error"); }
    setLoading(false);
  };

  const handleDelete = async () => {
    if (deleteId === null) return;
    try { await fetch(`/api/transactions/${deleteId}`, { method: "DELETE" }); toast.success("Deleted"); load(); } catch { toast.error("Failed"); }
    setDeleteId(null);
  };

  // Computed summaries
  const totalDebit = useMemo(() => ledger.reduce((s, e) => s + e.debit, 0), [ledger]);
  const totalCredit = useMemo(() => ledger.reduce((s, e) => s + e.credit, 0), [ledger]);

  const handleExcelExport = () => {
    if (!ledger.length) return;
    const rows = ledger.map((e, i) => ({
      "#": i + 1,
      Date: new Date(e.date).toLocaleDateString("en-IN"),
      "Bill No": e.billNo || "—",
      Type: e.txnType,
      Mode: e.paymentMode,
      "Debit (₹)": e.debit || "",
      "Credit (₹)": e.credit || "",
      "Balance (₹)": e.balance,
    }));
    exportToExcel(rows, `Ledger_${partyName.replace(/\s+/g, "_")}`, "Ledger");
    toast.success("Excel exported");
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      {/* ─── Header ─── */}
      <motion.div {...fadeUp} className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-2">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-50">Party Ledger</h1>
          <p className="mt-1 text-sm text-zinc-600 dark:text-zinc-300">
            {partyName ? `${partyName} • ${totalCount} transactions` : "Select a party to view statement"}
          </p>
        </div>
        <div className="flex gap-2">
          <Button variant="outline" size="sm" onClick={() => { if (ledger.length) { exportLedgerPdf(partyName, ledger, totalBalance); toast.success("PDF exported"); } }} disabled={!ledger.length}>
            <FileText className="mr-1.5 h-3.5 w-3.5" />PDF
          </Button>
          <Button variant="outline" size="sm" onClick={handleExcelExport} disabled={!ledger.length}>
            <Download className="mr-1.5 h-3.5 w-3.5" />Excel
          </Button>
          <Button variant="outline" size="sm" onClick={() => window.print()}>
            <Printer className="mr-1.5 h-3.5 w-3.5" />Print
          </Button>
        </div>
      </motion.div>

      {/* ─── Filters ─── */}
      <motion.div {...fadeUp}>
        <Card className="p-4">
          <div className="flex flex-wrap items-end gap-4">
            <div className="flex-1 min-w-[240px]">
              <label className="mb-1.5 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Party</label>
              <PartySearchDropdown parties={parties} value={party} onChange={setParty} onSubmit={load} />
            </div>
            <div>
              <label className="mb-1.5 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">From</label>
              <Input type="date" value={start} onChange={(e) => setStart(e.target.value)} className="w-40" />
            </div>
            <div>
              <label className="mb-1.5 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">To</label>
              <Input type="date" value={end} onChange={(e) => setEnd(e.target.value)} className="w-40" />
            </div>
            <Button size="sm" onClick={load} disabled={loading}>{loading ? "Loading..." : "Show Ledger"}</Button>
            <Button variant="secondary" size="sm" onClick={() => { const e = new Date(); setStart(new Date(e.getTime() - 29*86400000).toISOString().split("T")[0]); setEnd(e.toISOString().split("T")[0]); }}>30 Days</Button>
            <Button variant="secondary" size="sm" onClick={() => { const e = new Date(); setStart(new Date(e.getTime() - 89*86400000).toISOString().split("T")[0]); setEnd(e.toISOString().split("T")[0]); }}>90 Days</Button>
            <Button variant="ghost" size="sm" onClick={() => { setStart(""); setEnd(""); }}>All</Button>
          </div>
        </Card>
      </motion.div>

      {/* ─── Summary Cards (visible when data loaded) ─── */}
      {ledger.length > 0 && (
        <motion.div {...fadeUp} className="grid grid-cols-2 gap-3 sm:grid-cols-5">
          <Card className="p-3 text-center">
            <p className="text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Entries</p>
            <p className="mt-0.5 text-lg font-bold tabular-nums text-zinc-900 dark:text-zinc-50">{totalCount}</p>
          </Card>
          {openingBalance !== 0 && (
            <Card className="p-3 text-center">
              <p className="text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Opening</p>
              <p className={`mt-0.5 text-lg font-bold tabular-nums ${openingBalance >= 0 ? "text-red-600" : "text-emerald-600"}`}>
                ₹{fmt(Math.abs(openingBalance))}
              </p>
            </Card>
          )}
          <Card className="p-3 text-center">
            <p className="text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Total Debit</p>
            <p className="mt-0.5 text-lg font-bold tabular-nums text-red-600">₹{fmt(totalDebit)}</p>
          </Card>
          <Card className="p-3 text-center">
            <p className="text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Total Credit</p>
            <p className="mt-0.5 text-lg font-bold tabular-nums text-emerald-600">₹{fmt(totalCredit)}</p>
          </Card>
          <Card className="p-3 text-center">
            <p className="text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Closing</p>
            <p className={`mt-0.5 text-lg font-bold tabular-nums ${totalBalance >= 0 ? "text-red-600" : "text-emerald-600"}`}>
              ₹{fmt(Math.abs(totalBalance))} {totalBalance >= 0 ? "Dr" : "Cr"}
            </p>
          </Card>
        </motion.div>
      )}

      {/* ─── Table ─── */}
      <motion.div {...fadeUp} className="relative">
        {loading ? (
          <TableSkeleton rows={8} cols={7} />
        ) : ledger.length === 0 ? (
          <div className="rounded-xl border border-zinc-200 dark:border-zinc-700 bg-white dark:bg-zinc-900/60">
            <div className="flex flex-col items-center gap-2 py-16">
              <div className="text-3xl">📖</div>
              <p className="text-sm text-zinc-600 dark:text-zinc-300">Select a party and click &quot;Show Ledger&quot;</p>
              <p className="text-xs text-zinc-500">Start typing in the Party field to see suggestions</p>
            </div>
          </div>
        ) : (
          <>
            {/* Scrollable table container with max height */}
            <div
              ref={tableContainerRef}
              className="overflow-auto rounded-xl border border-zinc-300 dark:border-zinc-700 bg-white dark:bg-zinc-900/60 shadow-sm max-h-[70vh]"
            >
              <table className="w-full border-collapse font-mono text-[13px]">
                {/* Sticky Header */}
                <thead className="sticky top-0 z-10">
                  <tr className="bg-zinc-100 dark:bg-zinc-800">
                    <th className="border-b border-r border-zinc-300 dark:border-zinc-700 px-2 py-2.5 text-center text-[10px] font-semibold uppercase tracking-wider text-zinc-500 w-[40px]">#</th>
                    <th className="border-b border-r border-zinc-300 dark:border-zinc-700 px-3 py-2.5 text-left text-[10px] font-semibold uppercase tracking-wider text-zinc-500">Date</th>
                    <th className="border-b border-r border-zinc-300 dark:border-zinc-700 px-3 py-2.5 text-left text-[10px] font-semibold uppercase tracking-wider text-zinc-500 hidden sm:table-cell">Bill</th>
                    <th className="border-b border-r border-zinc-300 dark:border-zinc-700 px-3 py-2.5 text-left text-[10px] font-semibold uppercase tracking-wider text-zinc-500">Type</th>
                    <th className="border-b border-r border-zinc-300 dark:border-zinc-700 px-3 py-2.5 text-left text-[10px] font-semibold uppercase tracking-wider text-zinc-500 hidden md:table-cell">Mode</th>
                    <th className="border-b border-r border-zinc-300 dark:border-zinc-700 px-3 py-2.5 text-right text-[10px] font-semibold uppercase tracking-wider text-red-500">Debit</th>
                    <th className="border-b border-r border-zinc-300 dark:border-zinc-700 px-3 py-2.5 text-right text-[10px] font-semibold uppercase tracking-wider text-emerald-600">Credit</th>
                    <th className="border-b border-r border-zinc-300 dark:border-zinc-700 px-3 py-2.5 text-right text-[10px] font-semibold uppercase tracking-wider text-zinc-500">Balance</th>
                    <th className="border-b border-zinc-300 dark:border-zinc-700 px-2 py-2.5 text-center text-[10px] font-semibold uppercase tracking-wider text-zinc-500 w-[52px]">Act</th>
                  </tr>
                </thead>

                <tbody>
                  {ledger.map((e, i) => (
                    <tr key={e.txnId} className="hover:bg-blue-50/40 dark:hover:bg-zinc-800/40 transition-colors">
                      <td className="border-b border-r border-zinc-200 dark:border-zinc-700/60 px-2 py-2 text-center text-[10px] text-zinc-400 tabular-nums">{i + 1}</td>
                      <td className="border-b border-r border-zinc-200 dark:border-zinc-700/60 px-3 py-2 text-xs text-zinc-700 dark:text-zinc-300 tabular-nums whitespace-nowrap">
                        {new Date(e.date).toLocaleDateString("en-IN", { day: "2-digit", month: "short", year: "2-digit" })}
                      </td>
                      <td className="border-b border-r border-zinc-200 dark:border-zinc-700/60 px-3 py-2 text-xs text-zinc-600 dark:text-zinc-400 font-semibold hidden sm:table-cell">{e.billNo || "—"}</td>
                      <td className="border-b border-r border-zinc-200 dark:border-zinc-700/60 px-3 py-2">
                        <Badge variant={e.txnType === "Sale" ? "success" : e.txnType === "Expense" ? "danger" : e.txnType === "Receipt" ? "info" : "default"} className="text-[10px]">{e.txnType}</Badge>
                      </td>
                      <td className="border-b border-r border-zinc-200 dark:border-zinc-700/60 px-3 py-2 text-xs text-zinc-500 hidden md:table-cell">{e.paymentMode}</td>
                      <td className="border-b border-r border-zinc-200 dark:border-zinc-700/60 px-3 py-2 text-right tabular-nums font-medium text-red-600 dark:text-red-400">{e.debit > 0 ? `₹${fmt(e.debit)}` : ""}</td>
                      <td className="border-b border-r border-zinc-200 dark:border-zinc-700/60 px-3 py-2 text-right tabular-nums font-medium text-emerald-600 dark:text-emerald-400">{e.credit > 0 ? `₹${fmt(e.credit)}` : ""}</td>
                      <td className="border-b border-r border-zinc-200 dark:border-zinc-700/60 px-3 py-2 text-right tabular-nums font-semibold text-zinc-900 dark:text-zinc-100 whitespace-nowrap">
                        ₹{fmt(Math.abs(e.balance))} {e.balance >= 0 ? "Dr" : "Cr"}
                      </td>
                      <td className="border-b border-zinc-200 dark:border-zinc-700/60 px-1 py-2 text-center">
                        <div className="flex justify-center gap-0.5">
                          <Button variant="ghost" size="icon" className="h-6 w-6" onClick={() => setEditTxn({ txnId: e.txnId, txnDate: e.date, billNo: e.billNo, txnType: e.txnType, paymentMode: e.paymentMode, amount: String(e.debit || e.credit), party: { name: partyName } })}>
                            <Pencil className="h-3 w-3" />
                          </Button>
                          <Button variant="ghost" size="icon" className="h-6 w-6 text-red-500" onClick={() => setDeleteId(e.txnId)}>
                            <Trash2 className="h-3 w-3" />
                          </Button>
                        </div>
                      </td>
                    </tr>
                  ))}
                </tbody>

                {/* Sticky Footer Totals */}
                <tfoot className="sticky bottom-0 z-10">
                  <tr className="bg-zinc-100 dark:bg-zinc-800 font-bold">
                    <td colSpan={5} className="border-t border-r border-zinc-300 dark:border-zinc-700 px-3 py-3 text-[10px] font-semibold uppercase tracking-wider text-zinc-600 dark:text-zinc-400">
                      Total ({totalCount} entries)
                    </td>
                    <td className="border-t border-r border-zinc-300 dark:border-zinc-700 px-3 py-3 text-right tabular-nums text-red-600 dark:text-red-400">₹{fmt(totalDebit)}</td>
                    <td className="border-t border-r border-zinc-300 dark:border-zinc-700 px-3 py-3 text-right tabular-nums text-emerald-600 dark:text-emerald-400">₹{fmt(totalCredit)}</td>
                    <td className="border-t border-r border-zinc-300 dark:border-zinc-700 px-3 py-3 text-right tabular-nums text-zinc-900 dark:text-zinc-100 whitespace-nowrap">
                      ₹{fmt(Math.abs(totalBalance))} {totalBalance >= 0 ? "Dr" : "Cr"}
                    </td>
                    <td className="border-t border-zinc-300 dark:border-zinc-700 px-2 py-3"></td>
                  </tr>
                </tfoot>
              </table>
            </div>

            {/* Scroll to top FAB */}
            <AnimatePresence>
              {showScrollTop && (
                <motion.div
                  initial={{ opacity: 0, scale: 0.8 }} animate={{ opacity: 1, scale: 1 }} exit={{ opacity: 0, scale: 0.8 }}
                  className="absolute bottom-4 right-4 z-20"
                >
                  <Button
                    variant="default" size="icon" className="h-9 w-9 rounded-full shadow-lg"
                    onClick={() => tableContainerRef.current?.scrollTo({ top: 0, behavior: "smooth" })}
                  >
                    <ArrowUp className="h-4 w-4" />
                  </Button>
                </motion.div>
              )}
            </AnimatePresence>
          </>
        )}
      </motion.div>

      <EditTransactionModal transaction={editTxn} open={!!editTxn} onClose={() => setEditTxn(null)} onSaved={load} />
      <ConfirmDialog open={deleteId !== null} title="Delete Transaction" message="Permanently delete this entry?" variant="danger" confirmLabel="Delete" onConfirm={handleDelete} onCancel={() => setDeleteId(null)} />
    </motion.div>
  );
}
