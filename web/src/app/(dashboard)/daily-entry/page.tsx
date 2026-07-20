"use client";

import { useState, useEffect, useCallback, useRef } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Select } from "@/components/ui/select";
import { Badge } from "@/components/ui/badge";
import { TableSkeleton } from "@/components/ui/skeleton";
import { ConfirmDialog } from "@/components/ui/confirm-dialog";
import {
  Table, TableHeader, TableBody, TableRow, TableHead, TableCell,
} from "@/components/ui/data-table";
import { Plus, ChevronLeft, ChevronRight, Search, Pencil, Trash2 } from "lucide-react";

interface Transaction {
  txnId: number;
  txnDate: string;
  billNo: string | null;
  txnType: string;
  paymentMode: string;
  amount: string;
  party: { name: string };
}

interface PartyOption { name: string; normalizedName: string; }

const fadeUp = {
  initial: { opacity: 0, y: 12 },
  animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } },
};

function fmt(n: number | string): string {
  return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2, maximumFractionDigits: 2 }).format(Number(n));
}

export default function DailyEntryPage() {
  const [transactions, setTransactions] = useState<Transaction[]>([]);
  const [parties, setParties] = useState<PartyOption[]>([]);
  const [page, setPage] = useState(1);
  const [totalPages, setTotalPages] = useState(1);
  const [loading, setLoading] = useState(true);
  const [search, setSearch] = useState("");

  // Form state
  const [date, setDate] = useState(new Date().toISOString().split("T")[0]);
  const [billNo, setBillNo] = useState("");
  const [party, setParty] = useState("Customer");
  const [txnType, setTxnType] = useState("Sale");
  const [mode, setMode] = useState("Credit");
  const [amount, setAmount] = useState("");
  const [saving, setSaving] = useState(false);

  // Refs for Enter-key navigation (like Electron app)
  const dateRef = useRef<HTMLInputElement>(null);
  const billRef = useRef<HTMLInputElement>(null);
  const partyRef = useRef<HTMLInputElement>(null);
  const typeRef = useRef<HTMLSelectElement>(null);
  const modeRef = useRef<HTMLSelectElement>(null);
  const amountRef = useRef<HTMLInputElement>(null);

  // Edit modal
  const [editId, setEditId] = useState<number | null>(null);
  const [editField, setEditField] = useState("amount");
  const [editValue, setEditValue] = useState("");

  // Delete confirm
  const [deleteId, setDeleteId] = useState<number | null>(null);

  const loadTransactions = useCallback(async (p: number) => {
    setLoading(true);
    try {
      const res = await fetch(`/api/transactions?page=${p}&limit=50`);
      const data = await res.json();
      setTransactions(data.transactions || []);
      setTotalPages(data.totalPages || 1);
      setPage(p);
    } catch { /* ignore */ }
    setLoading(false);
  }, []);

  const loadParties = async () => {
    try {
      const res = await fetch("/api/parties");
      const data = await res.json();
      setParties(data.parties || []);
    } catch { /* ignore */ }
  };

  useEffect(() => { loadTransactions(1); loadParties(); }, [loadTransactions]);

  // Enter-key navigation: Date → Bill → Party → Type → Mode → Amount → Save
  const handleEntryNav = (e: React.KeyboardEvent, nextRef: React.RefObject<HTMLInputElement | HTMLSelectElement | null>) => {
    if (e.key === "Enter") {
      e.preventDefault();
      nextRef.current?.focus();
    }
  };

  const handleAmountKey = (e: React.KeyboardEvent) => {
    if (e.key === "Enter") {
      e.preventDefault();
      handleAdd();
    }
  };

  const handleAdd = async () => {
    if (!amount || parseFloat(amount) <= 0) {
      toast.error("Enter a valid amount");
      amountRef.current?.focus();
      return;
    }
    setSaving(true);
    try {
      const res = await fetch("/api/transactions", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          txnDate: date,
          billNo: billNo || undefined,
          party,
          txnType,
          paymentMode: mode,
          amount: parseFloat(amount),
        }),
      });
      if (res.ok) {
        toast.success("Transaction added", { description: `₹${fmt(amount)} • ${party} • ${txnType}` });
        setAmount("");
        setBillNo("");
        loadTransactions(1);
        // Focus back to bill field for rapid entry
        billRef.current?.focus();
      } else {
        const err = await res.json();
        toast.error(err.error || "Failed to add transaction");
      }
    } catch { toast.error("Network error"); }
    setSaving(false);
  };

  const handleDelete = async () => {
    if (deleteId === null) return;
    try {
      const res = await fetch(`/api/transactions/${deleteId}`, { method: "DELETE" });
      if (res.ok) {
        toast.success("Transaction deleted");
        loadTransactions(page);
      } else toast.error("Failed to delete");
    } catch { toast.error("Network error"); }
    setDeleteId(null);
  };

  const handleEdit = async () => {
    if (editId === null) return;
    try {
      const res = await fetch(`/api/transactions/${editId}`, {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ field: editField, value: editValue }),
      });
      if (res.ok) {
        toast.success("Transaction updated");
        setEditId(null);
        loadTransactions(page);
      } else {
        const err = await res.json();
        toast.error(err.error || "Failed to update");
      }
    } catch { toast.error("Network error"); }
  };

  const filtered = search
    ? transactions.filter((t) =>
        t.party.name.toLowerCase().includes(search.toLowerCase()) ||
        (t.billNo || "").toLowerCase().includes(search.toLowerCase()) ||
        t.txnType.toLowerCase().includes(search.toLowerCase())
      )
    : transactions;

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-2">
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Daily Transactions</h1>
          <p className="mt-0.5 text-sm text-zinc-500">{loading ? "Loading..." : `${transactions.length} entries`}</p>
        </div>
        <Badge variant="info">Page {page}/{totalPages}</Badge>
      </motion.div>

      {/* Entry Form with Enter-key navigation */}
      <motion.div {...fadeUp}>
        <Card className="p-4">
          <div className="grid grid-cols-1 gap-3 sm:grid-cols-2 lg:grid-cols-6 items-end">
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Date</label>
              <Input ref={dateRef} type="date" value={date} onChange={(e) => setDate(e.target.value)} onKeyDown={(e) => handleEntryNav(e, billRef)} />
            </div>
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Bill / Ref</label>
              <Input ref={billRef} placeholder="Bill No" value={billNo} onChange={(e) => setBillNo(e.target.value)} onKeyDown={(e) => handleEntryNav(e, partyRef)} />
            </div>
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Party</label>
              <Input ref={partyRef} placeholder="Select Party" value={party} onChange={(e) => setParty(e.target.value)} list="party-list" onKeyDown={(e) => handleEntryNav(e, typeRef)} onFocus={(e) => { if (e.target.value === "Customer") e.target.select(); }} />
              <datalist id="party-list">{parties.map((p) => <option key={p.normalizedName} value={p.name} />)}</datalist>
            </div>
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Type</label>
              <Select ref={typeRef} value={txnType} onChange={(e) => setTxnType(e.target.value)} onKeyDown={(e) => handleEntryNav(e, modeRef)}>
                <option>Sale</option><option>Sale Return</option><option>Expense</option><option>Receipt</option>
              </Select>
            </div>
            <div>
              <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Mode</label>
              <Select ref={modeRef} value={mode} onChange={(e) => setMode(e.target.value)} onKeyDown={(e) => handleEntryNav(e, amountRef)}>
                <option>Credit</option><option>Cash</option><option>UPI</option><option>Bank</option>
              </Select>
            </div>
            <div className="flex gap-2">
              <div className="flex-1">
                <label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Amount</label>
                <Input ref={amountRef} type="number" placeholder="0.00" min="0" step="1" value={amount} onChange={(e) => setAmount(e.target.value)} onKeyDown={handleAmountKey} />
              </div>
              <Button className="mt-auto h-10 w-10 p-0" size="icon" onClick={handleAdd} disabled={saving}>
                <Plus className="h-4 w-4" />
              </Button>
            </div>
          </div>
          <p className="mt-2 text-right text-[11px] text-zinc-400">
            Press <kbd className="rounded bg-zinc-100 px-1.5 py-0.5 text-[10px] font-medium dark:bg-zinc-800">Enter</kbd> to navigate fields → <kbd className="rounded bg-zinc-100 px-1.5 py-0.5 text-[10px] font-medium dark:bg-zinc-800">Enter</kbd> in Amount to Save
          </p>
        </Card>
      </motion.div>

      {/* Filters + Pagination */}
      <motion.div {...fadeUp} className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-2">
        <div className="flex items-center gap-2">
          <kbd className="text-[10px] text-zinc-400 bg-zinc-100 dark:bg-zinc-800 px-1.5 py-0.5 rounded">Ctrl+K</kbd>
          <span className="text-[11px] text-zinc-400">Search everywhere</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="relative">
            <Search className="absolute left-2.5 top-1/2 h-3.5 w-3.5 -translate-y-1/2 text-zinc-400" />
            <Input placeholder="Filter..." className="h-8 w-40 sm:w-48 pl-8 text-xs" value={search} onChange={(e) => setSearch(e.target.value)} />
          </div>
          <Button variant="outline" size="icon" className="h-8 w-8" onClick={() => loadTransactions(Math.max(1, page - 1))} disabled={page <= 1}><ChevronLeft className="h-4 w-4" /></Button>
          <span className="min-w-[50px] text-center text-xs text-zinc-500">{page}/{totalPages}</span>
          <Button variant="outline" size="icon" className="h-8 w-8" onClick={() => loadTransactions(Math.min(totalPages, page + 1))} disabled={page >= totalPages}><ChevronRight className="h-4 w-4" /></Button>
        </div>
      </motion.div>

      {/* Table */}
      <motion.div {...fadeUp}>
        {loading ? <TableSkeleton rows={8} cols={7} /> : (
          <Table>
            <TableHeader>
              <TableRow>
                <TableHead>Date</TableHead>
                <TableHead className="hidden sm:table-cell">Bill No</TableHead>
                <TableHead>Party</TableHead>
                <TableHead className="hidden md:table-cell">Type</TableHead>
                <TableHead className="hidden md:table-cell">Mode</TableHead>
                <TableHead className="text-right">Amount</TableHead>
                <TableHead className="w-20 text-center">Actions</TableHead>
              </TableRow>
            </TableHeader>
            <TableBody>
              {filtered.length === 0 ? (
                <TableRow><TableCell colSpan={7} className="h-32 text-center"><p className="text-sm text-zinc-500">No transactions</p><p className="text-xs text-zinc-400 mt-1">Add your first entry above</p></TableCell></TableRow>
              ) : filtered.map((t) => (
                <TableRow key={t.txnId}>
                  <TableCell className="text-xs">{new Date(t.txnDate).toLocaleDateString("en-IN")}</TableCell>
                  <TableCell className="hidden sm:table-cell">{t.billNo || "—"}</TableCell>
                  <TableCell className="font-medium max-w-[120px] truncate">{t.party.name}</TableCell>
                  <TableCell className="hidden md:table-cell"><Badge variant={t.txnType === "Sale" ? "success" : t.txnType === "Expense" ? "danger" : t.txnType === "Receipt" ? "info" : "default"}>{t.txnType}</Badge></TableCell>
                  <TableCell className="hidden md:table-cell"><Badge>{t.paymentMode}</Badge></TableCell>
                  <TableCell className="text-right font-mono font-medium">₹{fmt(t.amount)}</TableCell>
                  <TableCell className="text-center">
                    <div className="flex justify-center gap-0.5">
                      <Button variant="ghost" size="icon" className="h-7 w-7" onClick={() => { setEditId(t.txnId); setEditField("amount"); setEditValue(String(t.amount)); }}><Pencil className="h-3.5 w-3.5" /></Button>
                      <Button variant="ghost" size="icon" className="h-7 w-7 text-red-500 hover:text-red-700" onClick={() => setDeleteId(t.txnId)}><Trash2 className="h-3.5 w-3.5" /></Button>
                    </div>
                  </TableCell>
                </TableRow>
              ))}
            </TableBody>
          </Table>
        )}
      </motion.div>

      {/* Edit Modal */}
      {editId !== null && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/40 backdrop-blur-sm" onClick={() => setEditId(null)}>
          <motion.div
            initial={{ opacity: 0, scale: 0.95 }}
            animate={{ opacity: 1, scale: 1 }}
            transition={{ type: "spring" as const, bounce: 0, duration: 0.25 }}
            className="w-full max-w-sm rounded-2xl border border-zinc-200 bg-white p-6 shadow-2xl dark:border-zinc-700 dark:bg-zinc-900"
            onClick={(e) => e.stopPropagation()}
          >
            <h3 className="text-lg font-semibold text-zinc-900 dark:text-zinc-100 mb-4">Edit Transaction</h3>
            <div className="space-y-3">
              <div>
                <label className="mb-1 block text-xs font-medium text-zinc-500">Field</label>
                <Select value={editField} onChange={(e) => setEditField(e.target.value)}>
                  <option value="amount">Amount</option><option value="bill_no">Bill No</option><option value="txn_date">Date</option><option value="payment_mode">Mode</option>
                </Select>
              </div>
              <div>
                <label className="mb-1 block text-xs font-medium text-zinc-500">New Value</label>
                <Input value={editValue} onChange={(e) => setEditValue(e.target.value)} onKeyDown={(e) => { if (e.key === "Enter") handleEdit(); }} autoFocus />
              </div>
            </div>
            <div className="mt-5 flex gap-2 justify-end">
              <Button variant="outline" onClick={() => setEditId(null)}>Cancel</Button>
              <Button onClick={handleEdit}>Save</Button>
            </div>
          </motion.div>
        </div>
      )}

      {/* Delete Confirmation */}
      <ConfirmDialog
        open={deleteId !== null}
        title="Delete Transaction"
        message="Are you sure you want to permanently delete this transaction? This action cannot be undone."
        variant="danger"
        confirmLabel="Delete"
        onConfirm={handleDelete}
        onCancel={() => setDeleteId(null)}
      />
    </motion.div>
  );
}
