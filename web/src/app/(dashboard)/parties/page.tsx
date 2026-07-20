"use client";

import { useState, useEffect } from "react";
import { motion } from "framer-motion";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Select } from "@/components/ui/select";
import { Badge } from "@/components/ui/badge";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { UserPlus } from "lucide-react";

interface PartyItem { partyId: number; name: string; type: string; creditAllowed: boolean; }
const fadeUp = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } } };

export default function PartiesPage() {
  const [parties, setParties] = useState<PartyItem[]>([]);
  const [name, setName] = useState("");
  const [type, setType] = useState("Customer");
  const [credit, setCredit] = useState(false);
  const [saving, setSaving] = useState(false);

  const load = () => { fetch("/api/parties").then(r => r.json()).then(d => setParties(d.parties || [])).catch(() => {}); };
  useEffect(() => { load(); }, []);

  const handleCreate = async () => {
    if (!name.trim()) return;
    setSaving(true);
    try {
      const res = await fetch("/api/parties", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ name: name.trim(), type, creditAllowed: credit }) });
      if (res.ok) { setName(""); load(); } else { const d = await res.json(); alert(d.error || "Failed"); }
    } catch { alert("Network error"); }
    setSaving(false);
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-6 max-w-3xl">
      <motion.div {...fadeUp}><h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Parties</h1><p className="mt-0.5 text-sm text-zinc-500">{parties.length} parties registered</p></motion.div>
      <motion.div {...fadeUp}><Card><CardHeader><CardTitle className="flex items-center gap-2"><UserPlus className="h-5 w-5" /> Add New Party</CardTitle></CardHeader><CardContent className="space-y-4">
        <div><label className="mb-1.5 block text-xs font-medium text-zinc-600">Party Name</label><Input placeholder="Enter name..." value={name} onChange={(e) => setName(e.target.value)} /></div>
        <div><label className="mb-1.5 block text-xs font-medium text-zinc-600">Type</label><Select value={type} onChange={(e) => setType(e.target.value)}><option value="Customer">Customer</option><option value="Credit Customer">Credit Customer</option><option value="Supplier">Supplier</option><option value="Expense Account">Expense Account</option><option value="Bank">Bank Account</option></Select></div>
        <label className="flex items-center gap-2 cursor-pointer"><input type="checkbox" checked={credit} onChange={(e) => setCredit(e.target.checked)} className="h-4 w-4 rounded border-zinc-300" /><span className="text-sm text-zinc-700">Allow Credit</span></label>
        <Button className="w-full" onClick={handleCreate} disabled={saving}>{saving ? "Creating..." : "Create Party"}</Button>
      </CardContent></Card></motion.div>
      <motion.div {...fadeUp}><Table><TableHeader><TableRow><TableHead>Name</TableHead><TableHead>Type</TableHead><TableHead>Credit</TableHead></TableRow></TableHeader><TableBody>
        {parties.length === 0 ? (<TableRow><TableCell colSpan={3} className="h-20 text-center text-sm text-zinc-500">No parties</TableCell></TableRow>) : parties.map((p) => (<TableRow key={p.partyId}><TableCell className="font-medium">{p.name}</TableCell><TableCell><Badge>{p.type}</Badge></TableCell><TableCell>{p.creditAllowed ? "Yes" : "—"}</TableCell></TableRow>))}
      </TableBody></Table></motion.div>
    </motion.div>
  );
}
