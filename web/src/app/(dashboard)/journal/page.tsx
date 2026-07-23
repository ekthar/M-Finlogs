"use client";

import { useState, useEffect, useRef } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Badge } from "@/components/ui/badge";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { ArrowLeftRight, Plus, BookOpen } from "lucide-react";
import { springs } from "@/lib/design-tokens";

interface JournalEntry {
  jvNo: string;
  date: string;
  entries: { txnId: number; party: string; type: string; amount: number }[];
}

interface PartyOption { name: string; normalizedName: string; }
const itemVariants = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: springs.default } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }

export default function JournalPage() {
  const { companyId } = useApp();
  const [journals, setJournals] = useState<JournalEntry[]>([]);
  const [parties, setParties] = useState<PartyOption[]>([]);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);

  // Form
  const [date, setDate] = useState(new Date().toISOString().split("T")[0]);
  const [debitParty, setDebitParty] = useState("");
  const [creditParty, setCreditParty] = useState("");
  const [amount, setAmount] = useState("");
  const [narration, setNarration] = useState("");
  const amountRef = useRef<HTMLInputElement>(null);

  const loadJournals = async () => {
    setLoading(true);
    try {
      const res = await fetch(`/api/journal?companyId=${companyId}&limit=50`);
      const data = await res.json();
      setJournals(data.journals || []);
    } catch {}
    setLoading(false);
  };

  useEffect(() => {
    loadJournals();
    fetch(`/api/parties?companyId=${companyId}`).then(r => r.json()).then(d => setParties(d.parties || [])).catch(() => {});
  }, [companyId]);

  const handleSubmit = async () => {
    if (!debitParty.trim()) { toast.error("Select Debit party"); return; }
    if (!creditParty.trim()) { toast.error("Select Credit party"); return; }
    if (debitParty.trim().toLowerCase() === creditParty.trim().toLowerCase()) { toast.error("Debit and Credit parties must be different"); return; }
    const amt = parseFloat(amount);
    if (!amt || amt <= 0) { toast.error("Enter valid amount"); amountRef.current?.focus(); return; }

    setSaving(true);
    try {
      const res = await fetch("/api/journal", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ companyId, date, debitParty: debitParty.trim(), creditParty: creditParty.trim(), amount: amt, narration: narration.trim() }),
      });
      const data = await res.json();
      if (res.ok) {
        toast.success(`Journal ${data.jvNo} created`);
        setDebitParty("");
        setCreditParty("");
        setAmount("");
        setNarration("");
        loadJournals();
      } else {
        toast.error(data.error || "Failed");
      }
    } catch { toast.error("Network error"); }
    setSaving(false);
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-6">
      <motion.div variants={itemVariants}>
        <h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-50">Journal Entries</h1>
        <p className="mt-1 text-sm text-zinc-600 dark:text-zinc-300">Double-entry journal vouchers — transfers, adjustments, corrections</p>
      </motion.div>

      {/* ═══ Entry Form ═══ */}
      <motion.div variants={itemVariants}>
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2 text-base">
              <ArrowLeftRight className="h-4 w-4" /> New Journal Voucher
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-4">
              <div>
                <label className="mb-1.5 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Date</label>
                <Input type="date" value={date} onChange={e => setDate(e.target.value)} />
              </div>
              <div>
                <label className="mb-1.5 block text-[11px] font-medium uppercase tracking-wider text-red-500">Debit (Dr) Party</label>
                <Input value={debitParty} onChange={e => setDebitParty(e.target.value)} placeholder="Who owes more..." list="jv-parties" />
              </div>
              <div>
                <label className="mb-1.5 block text-[11px] font-medium uppercase tracking-wider text-emerald-600">Credit (Cr) Party</label>
                <Input value={creditParty} onChange={e => setCreditParty(e.target.value)} placeholder="Who pays/reduces..." list="jv-parties" />
              </div>
              <div>
                <label className="mb-1.5 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Amount (₹)</label>
                <Input ref={amountRef} type="number" value={amount} onChange={e => setAmount(e.target.value)} placeholder="0.00" min="0.50" onKeyDown={e => { if (e.key === "Enter") handleSubmit(); }} />
              </div>
            </div>
            <div>
              <label className="mb-1.5 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Narration (optional)</label>
              <Input value={narration} onChange={e => setNarration(e.target.value)} placeholder="Reason for this journal entry..." onKeyDown={e => { if (e.key === "Enter") handleSubmit(); }} />
            </div>

            {/* Visual representation */}
            {debitParty && creditParty && amount && (
              <div className="rounded-xl bg-zinc-50 dark:bg-zinc-800/50 p-4 border border-zinc-200/50 dark:border-zinc-700/50">
                <p className="text-xs font-medium text-zinc-500 mb-2">Preview:</p>
                <div className="flex items-center gap-3 text-sm">
                  <div className="flex-1 rounded-lg bg-red-50 dark:bg-red-900/10 p-3 text-center border border-red-200/50 dark:border-red-800/30">
                    <p className="text-[10px] uppercase tracking-wider text-red-500 font-medium">Debit</p>
                    <p className="font-semibold text-red-700 dark:text-red-400 mt-0.5">{debitParty}</p>
                    <p className="text-xs font-mono text-red-600 mt-0.5">₹{fmt(parseFloat(amount) || 0)}</p>
                  </div>
                  <ArrowLeftRight className="h-5 w-5 text-zinc-400 flex-shrink-0" />
                  <div className="flex-1 rounded-lg bg-emerald-50 dark:bg-emerald-900/10 p-3 text-center border border-emerald-200/50 dark:border-emerald-800/30">
                    <p className="text-[10px] uppercase tracking-wider text-emerald-500 font-medium">Credit</p>
                    <p className="font-semibold text-emerald-700 dark:text-emerald-400 mt-0.5">{creditParty}</p>
                    <p className="text-xs font-mono text-emerald-600 mt-0.5">₹{fmt(parseFloat(amount) || 0)}</p>
                  </div>
                </div>
              </div>
            )}

            <Button className="w-full sm:w-auto" onClick={handleSubmit} disabled={saving}>
              <Plus className="mr-1.5 h-3.5 w-3.5" />
              {saving ? "Creating..." : "Create Journal Entry"}
            </Button>

            <datalist id="jv-parties">
              {parties.map(p => <option key={p.normalizedName} value={p.name} />)}
            </datalist>
          </CardContent>
        </Card>
      </motion.div>

      {/* ═══ Recent Journals ═══ */}
      <motion.div variants={itemVariants}>
        <div className="flex items-center gap-2 mb-3">
          <BookOpen className="h-4 w-4 text-zinc-500" />
          <h2 className="text-sm font-semibold text-zinc-700 dark:text-zinc-300">Recent Journal Vouchers</h2>
          <Badge className="ml-auto">{journals.length}</Badge>
        </div>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead>JV No</TableHead>
              <TableHead>Date</TableHead>
              <TableHead>Debit Party</TableHead>
              <TableHead>Credit Party</TableHead>
              <TableHead className="text-right">Amount</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            {journals.length === 0 ? (
              <TableRow>
                <TableCell colSpan={5} className="h-24 text-center">
                  <p className="text-sm text-zinc-500">{loading ? "Loading..." : "No journal entries yet"}</p>
                </TableCell>
              </TableRow>
            ) : journals.map(j => {
              const debit = j.entries.find(e => e.type === "Sale");
              const credit = j.entries.find(e => e.type === "Receipt");
              return (
                <TableRow key={j.jvNo}>
                  <TableCell className="font-mono text-xs font-semibold text-blue-600 dark:text-blue-400">{j.jvNo}</TableCell>
                  <TableCell className="text-xs">{new Date(j.date).toLocaleDateString("en-IN")}</TableCell>
                  <TableCell className="text-sm">
                    <span className="text-red-600 dark:text-red-400 font-medium">{debit?.party || "—"}</span>
                  </TableCell>
                  <TableCell className="text-sm">
                    <span className="text-emerald-600 dark:text-emerald-400 font-medium">{credit?.party || "—"}</span>
                  </TableCell>
                  <TableCell className="text-right font-mono font-semibold tabular-nums">₹{fmt(debit?.amount || credit?.amount || 0)}</TableCell>
                </TableRow>
              );
            })}
          </TableBody>
        </Table>
      </motion.div>
    </motion.div>
  );
}
