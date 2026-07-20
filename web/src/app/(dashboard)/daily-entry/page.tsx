"use client";

import { useState, useEffect, useCallback, useRef } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { enqueue, getPendingCount, startAutoSync, cleanupSynced } from "@/lib/offline-queue";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Select } from "@/components/ui/select";
import { Badge } from "@/components/ui/badge";
import { TableSkeleton } from "@/components/ui/skeleton";
import { ConfirmDialog } from "@/components/ui/confirm-dialog";
import { EditTransactionModal } from "@/components/edit-transaction-modal";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { Plus, ChevronLeft, ChevronRight, Search, Pencil, Trash2, Filter, WifiOff, CloudUpload } from "lucide-react";
import { springs } from "@/lib/design-tokens";

interface Transaction { txnId: number; txnDate: string; billNo: string | null; txnType: string; paymentMode: string; amount: string; party: { name: string; type: string }; }
interface PartyOption { name: string; normalizedName: string; }
const itemVariants = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: springs.default } };
function fmt(n: number | string) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2, maximumFractionDigits: 2 }).format(Number(n)); }

export default function DailyEntryPage() {
  const { companyId, financialYear } = useApp();
  const [transactions, setTransactions] = useState<Transaction[]>([]);
  const [parties, setParties] = useState<PartyOption[]>([]);
  const [page, setPage] = useState(1);
  const [totalPages, setTotalPages] = useState(1);
  const [total, setTotal] = useState(0);
  const [loading, setLoading] = useState(true);
  const [search, setSearch] = useState("");
  const [showFilters, setShowFilters] = useState(false);
  const [startDate, setStartDate] = useState("");
  const [endDate, setEndDate] = useState("");
  const [date, setDate] = useState(new Date().toISOString().split("T")[0]);
  const [billNo, setBillNo] = useState("");
  const [party, setParty] = useState("Customer");
  const [txnType, setTxnType] = useState("Sale");
  const [mode, setMode] = useState("Credit");
  const [amount, setAmount] = useState("");
  const [online, setOnline] = useState(true);
  const [pendingCount, setPendingCount] = useState(0);

  const billRef = useRef<HTMLInputElement>(null);
  const partyRef = useRef<HTMLInputElement>(null);
  const typeRef = useRef<HTMLSelectElement>(null);
  const modeRef = useRef<HTMLSelectElement>(null);
  const amountRef = useRef<HTMLInputElement>(null);
  const [editTxn, setEditTxn] = useState<Transaction | null>(null);
  const [deleteId, setDeleteId] = useState<number | null>(null);

  // Auto-sync setup
  useEffect(() => {
    setOnline(navigator.onLine);
    const goOnline = () => setOnline(true);
    const goOffline = () => { setOnline(false); toast.warning("Offline — entries safe locally"); };
    window.addEventListener("online", goOnline);
    window.addEventListener("offline", goOffline);
    const cleanup = startAutoSync();

    // When queue syncs, refresh the table
    const onSynced = () => {
      setPendingCount(getPendingCount());
      cleanupSynced();
      loadTransactions(1);
    };
    window.addEventListener("mfinlogs:synced", onSynced);

    return () => {
      window.removeEventListener("online", goOnline);
      window.removeEventListener("offline", goOffline);
      window.removeEventListener("mfinlogs:synced", onSynced);
      cleanup?.();
    };
  }, []);

  const loadTransactions = useCallback(async (p: number) => {
    setLoading(true);
    const params = new URLSearchParams({ page: String(p), limit: "50", companyId, financialYear });
    if (startDate) params.set("start", startDate);
    if (endDate) params.set("end", endDate);
    try {
      const res = await fetch(`/api/transactions?${params}`);
      const data = await res.json();
      setTransactions(data.transactions || []); setTotalPages(data.totalPages || 1); setTotal(data.total || 0); setPage(p);
    } catch {}
    setLoading(false);
    setPendingCount(getPendingCount());
  }, [companyId, financialYear, startDate, endDate]);

  useEffect(() => { loadTransactions(1); }, [loadTransactions]);
  useEffect(() => { fetch(`/api/parties?companyId=${companyId}`).then(r => r.json()).then(d => setParties(d.parties || [])).catch(() => {}); }, [companyId]);
  // Auto-load next bill number
  useEffect(() => {
    fetch(`/api/transactions/last-bill?companyId=${companyId}`)
      .then(r => r.json())
      .then(d => { if (d.nextBill && !billNo) setBillNo(d.nextBill); })
      .catch(() => {});
  }, [companyId]);

  const handleEntryNav = (e: React.KeyboardEvent, next: React.RefObject<HTMLInputElement | HTMLSelectElement | null>) => { if (e.key === "Enter") { e.preventDefault(); next.current?.focus(); } };

  /**
   * INSTANT SAVE — never waits for server.
   * 1. Add to queue (localStorage) → <1ms
   * 2. Show optimistic row in table → instant
   * 3. Queue worker syncs to server in background → non-blocking
   * 4. Server deduplicates by clientId → no duplicates ever
   */
  const handleAdd = () => {
    if (!amount || parseFloat(amount) <= 0) { toast.error("Enter a valid amount"); amountRef.current?.focus(); return; }
    const amt = parseFloat(amount);

    // Amount warning for very large values (>₹10 Lakh)
    if (amt > 1000000 && !window.confirm(`₹${amt.toLocaleString("en-IN")} is very large. Confirm?`)) return;

    // 1. Queue it (localStorage — survives offline, refresh, crash)
    const entry = enqueue({ txnDate: date, billNo: billNo || undefined, party, txnType, paymentMode: mode, amount: amt, companyId });

    // 2. Optimistic UI — show immediately
    const optimisticTxn: Transaction = {
      txnId: -Date.now(), txnDate: date, billNo: billNo || null,
      txnType, paymentMode: mode, amount: String(amt),
      party: { name: party, type: "" },
    };
    setTransactions((prev) => [optimisticTxn, ...prev]);
    setTotal((t) => t + 1);
    setPendingCount(getPendingCount());

    // 3. Feedback + reset form
    toast.success("Saved", { description: `₹${fmt(amt)} • ${party}` });
    setAmount(""); setBillNo(""); billRef.current?.focus();

    // 4. Background sync handles the rest (non-blocking)
    // Queue worker will POST and deduplicate automatically
  };

  const handleDelete = async () => {
    if (deleteId === null) return;
    const id = deleteId;
    setDeleteId(null);
    setTransactions(prev => prev.filter(t => t.txnId !== id));
    setTotal(t => t - 1);
    toast("Transaction deleted", { description: "Permanent in 10 seconds", duration: 10000, action: { label: "⟲ Undo", onClick: () => { loadTransactions(page); } } });
    setTimeout(async () => { try { await fetch(`/api/transactions/${id}`, { method: "DELETE" }); } catch {} }, 10000);
  };

  const filtered = search ? transactions.filter(t => t.party.name.toLowerCase().includes(search.toLowerCase()) || (t.billNo||"").toLowerCase().includes(search.toLowerCase())) : transactions;

  return (
    <motion.div initial="initial" animate="animate" className="space-y-4">
      <motion.div variants={itemVariants} className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-2">
        <div>
          <h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-100">Daily Transactions</h1>
          <p className="mt-0.5 text-sm text-zinc-500">{total.toLocaleString()} entries • FY {financialYear}</p>
        </div>
        <div className="flex items-center gap-2">
          {!online && <Badge variant="warning" className="gap-1"><WifiOff className="h-3 w-3"/>Offline</Badge>}
          {pendingCount > 0 && <Badge variant="info" className="gap-1"><CloudUpload className="h-3 w-3"/>{pendingCount} syncing</Badge>}
          <Button variant={showFilters ? "default" : "outline"} size="sm" onClick={() => setShowFilters(!showFilters)}><Filter className="h-3.5 w-3.5 mr-1"/>Filters</Button>
          <Badge>{page}/{totalPages}</Badge>
        </div>
      </motion.div>

      {showFilters && <Card className="p-3"><div className="flex flex-wrap items-end gap-3"><div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">From</label><Input type="date" value={startDate} onChange={e => setStartDate(e.target.value)} className="w-40"/></div><div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">To</label><Input type="date" value={endDate} onChange={e => setEndDate(e.target.value)} className="w-40"/></div><Button variant="secondary" size="sm" onClick={() => { const e=new Date(); setStartDate(new Date(e.getTime()-29*86400000).toISOString().split("T")[0]); setEndDate(e.toISOString().split("T")[0]); }}>Last 30</Button><Button variant="secondary" size="sm" onClick={() => { setStartDate(""); setEndDate(""); }}>All</Button><Button size="sm" onClick={() => loadTransactions(1)}>Apply</Button></div></Card>}

      <motion.div variants={itemVariants}><Card className="p-4"><div className="grid grid-cols-1 gap-3 sm:grid-cols-2 lg:grid-cols-6 items-end">
        <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Date</label><Input type="date" value={date} onChange={e => setDate(e.target.value)} onKeyDown={e => handleEntryNav(e, billRef)}/></div>
        <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Bill/Ref</label><Input ref={billRef} placeholder="Bill No" value={billNo} onChange={e => setBillNo(e.target.value)} onKeyDown={e => handleEntryNav(e, partyRef)}/></div>
        <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Party</label><Input ref={partyRef} value={party} onChange={e => setParty(e.target.value)} list="pdl" onKeyDown={e => handleEntryNav(e, typeRef)} onFocus={e => {if(e.target.value==="Customer") e.target.select();}}/><datalist id="pdl">{parties.map(p=><option key={p.normalizedName} value={p.name}/>)}</datalist></div>
        <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Type</label><Select ref={typeRef} value={txnType} onChange={e => setTxnType(e.target.value)} onKeyDown={e => handleEntryNav(e, modeRef)}><option>Sale</option><option>Sale Return</option><option>Expense</option><option>Receipt</option></Select></div>
        <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Mode</label><Select ref={modeRef} value={mode} onChange={e => setMode(e.target.value)} onKeyDown={e => handleEntryNav(e, amountRef)}><option>Credit</option><option>Cash</option><option>UPI</option><option>Bank</option></Select></div>
        <div className="flex gap-2"><div className="flex-1"><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Amount</label><Input ref={amountRef} type="number" placeholder="0.00" min="0" value={amount} onChange={e => setAmount(e.target.value)} onKeyDown={e => {if(e.key==="Enter"){e.preventDefault();handleAdd();}}}/></div><Button className="mt-auto h-9 w-9 p-0" size="icon" onClick={handleAdd}><Plus className="h-4 w-4"/></Button></div>
      </div><p className="mt-2 text-right text-[11px] text-zinc-400">Enter navigates → Enter in Amount saves instantly (works offline)</p></Card></motion.div>

      <div className="flex items-center justify-between"><div className="relative"><Search className="absolute left-2.5 top-1/2 h-3.5 w-3.5 -translate-y-1/2 text-zinc-400"/><Input placeholder="Filter..." className="h-8 w-40 sm:w-48 pl-8 text-xs" value={search} onChange={e => setSearch(e.target.value)}/></div><div className="flex items-center gap-1"><Button variant="outline" size="icon" className="h-8 w-8" onClick={() => loadTransactions(Math.max(1,page-1))} disabled={page<=1}><ChevronLeft className="h-4 w-4"/></Button><span className="min-w-[50px] text-center text-xs text-zinc-500">{page}/{totalPages}</span><Button variant="outline" size="icon" className="h-8 w-8" onClick={() => loadTransactions(Math.min(totalPages,page+1))} disabled={page>=totalPages}><ChevronRight className="h-4 w-4"/></Button></div></div>

      <motion.div variants={itemVariants}>{loading && transactions.length === 0 ? <TableSkeleton rows={8} cols={7}/> : <Table><TableHeader><TableRow><TableHead>Date</TableHead><TableHead className="hidden sm:table-cell">Bill</TableHead><TableHead>Party</TableHead><TableHead className="hidden md:table-cell">Type</TableHead><TableHead className="hidden md:table-cell">Mode</TableHead><TableHead className="text-right">Amount</TableHead><TableHead className="w-20 text-center">Edit</TableHead></TableRow></TableHeader><TableBody>
        {filtered.length===0 ? <TableRow><TableCell colSpan={7} className="h-32 text-center text-sm text-zinc-500">No transactions</TableCell></TableRow> : filtered.map(t => <TableRow key={t.txnId} className={t.txnId < 0 ? "opacity-50 bg-blue-50/20 dark:bg-blue-900/5" : ""}><TableCell className="text-xs">{new Date(t.txnDate).toLocaleDateString("en-IN")}</TableCell><TableCell className="hidden sm:table-cell text-xs">{t.billNo||"—"}</TableCell><TableCell className="font-medium max-w-[140px] truncate text-xs">{t.party.name}</TableCell><TableCell className="hidden md:table-cell"><Badge variant={t.txnType==="Sale"?"success":t.txnType==="Expense"?"danger":"info"}>{t.txnType}</Badge></TableCell><TableCell className="hidden md:table-cell"><Badge>{t.paymentMode}</Badge></TableCell><TableCell className="text-right font-mono text-xs font-medium">₹{fmt(t.amount)}</TableCell><TableCell className="text-center">{t.txnId > 0 ? <div className="flex justify-center gap-0.5"><Button variant="ghost" size="icon" className="h-7 w-7" onClick={()=>setEditTxn(t)}><Pencil className="h-3 w-3"/></Button><Button variant="ghost" size="icon" className="h-7 w-7 text-red-500" onClick={()=>setDeleteId(t.txnId)}><Trash2 className="h-3 w-3"/></Button></div> : <span className="text-[10px] text-blue-500 animate-pulse">syncing</span>}</TableCell></TableRow>)}
      </TableBody></Table>}</motion.div>

      <EditTransactionModal transaction={editTxn} open={!!editTxn} onClose={() => setEditTxn(null)} onSaved={() => loadTransactions(page)}/>
      <ConfirmDialog open={deleteId!==null} title="Delete Transaction" message="Permanently delete?" variant="danger" confirmLabel="Delete" onConfirm={handleDelete} onCancel={() => setDeleteId(null)}/>
    </motion.div>
  );
}
