"use client";

import { useState, useEffect } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Select } from "@/components/ui/select";
import { Badge } from "@/components/ui/badge";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { UserPlus, DollarSign } from "lucide-react";
import { springs } from "@/lib/design-tokens";

interface PartyItem { partyId: number; name: string; type: string; creditAllowed: boolean; }
const itemVariants = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: springs.default } };

export default function PartiesPage() {
  const { companyId } = useApp();
  const [parties, setParties] = useState<PartyItem[]>([]);
  const [name, setName] = useState("");
  const [type, setType] = useState("Customer");
  const [credit, setCredit] = useState(false);
  const [saving, setSaving] = useState(false);
  const [obParty, setObParty] = useState("");
  const [obAmount, setObAmount] = useState("");

  const load = () => { fetch(`/api/parties?companyId=${companyId}`).then(r=>r.json()).then(d=>setParties(d.parties||[])).catch(()=>{}); };
  useEffect(() => { load(); }, [companyId]);

  const handleCreate = async () => {
    if (!name.trim()) return;
    setSaving(true);
    const res = await fetch("/api/parties", { method: "POST", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ name: name.trim(), type, creditAllowed: credit, companyId }) });
    if (res.ok) { setName(""); load(); toast.success("Party created"); } else { const d = await res.json(); toast.error(d.error||"Failed"); }
    setSaving(false);
  };

  const saveOpeningBalance = async () => {
    if (!obParty || !obAmount) { toast.error("Select party and enter amount"); return; }
    const party = parties.find(p => p.name === obParty);
    if (!party) { toast.error("Party not found"); return; }
    const res = await fetch("/api/parties/opening-balance", { method: "POST", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ companyId, partyId: party.partyId, balance: parseFloat(obAmount) }) });
    if (res.ok) { toast.success(`Opening balance set for ${obParty}`); setObAmount(""); } else toast.error("Failed");
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-6 max-w-3xl">
      <motion.div variants={itemVariants}><h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-100">Parties</h1><p className="mt-0.5 text-sm text-zinc-500">{parties.length} parties registered</p></motion.div>

      {/* Create */}
      <motion.div variants={itemVariants}><Card><CardHeader><CardTitle className="flex items-center gap-2"><UserPlus className="h-4 w-4"/>Add Party</CardTitle></CardHeader><CardContent className="space-y-4">
        <div><label className="mb-1.5 block text-xs font-medium text-zinc-600">Name</label><Input placeholder="Enter name..." value={name} onChange={e=>setName(e.target.value)} onKeyDown={e=>{if(e.key==="Enter")handleCreate();}}/></div>
        <div><label className="mb-1.5 block text-xs font-medium text-zinc-600">Type</label><Select value={type} onChange={e=>setType(e.target.value)}><option value="Customer">Customer</option><option value="Credit Customer">Credit Customer</option><option value="Supplier">Supplier</option><option value="Expense Account">Expense Account</option><option value="Bank">Bank</option></Select></div>
        <label className="flex items-center gap-2 cursor-pointer"><input type="checkbox" checked={credit} onChange={e=>setCredit(e.target.checked)} className="h-4 w-4 rounded border-zinc-300"/><span className="text-sm text-zinc-700 dark:text-zinc-300">Allow Credit</span></label>
        <Button className="w-full" onClick={handleCreate} disabled={saving}>{saving?"Creating...":"Create Party"}</Button>
      </CardContent></Card></motion.div>

      {/* Opening Balance */}
      <motion.div variants={itemVariants}><Card><CardHeader><CardTitle className="flex items-center gap-2"><DollarSign className="h-4 w-4"/>Set Opening Balance</CardTitle></CardHeader><CardContent>
        <p className="text-xs text-zinc-500 mb-3">Set the starting balance for a party (carry-forward from previous FY)</p>
        <div className="grid grid-cols-1 gap-3 sm:grid-cols-3 items-end">
          <div><label className="mb-1 block text-xs font-medium text-zinc-600">Party</label><Input value={obParty} onChange={e=>setObParty(e.target.value)} list="ob-parties" placeholder="Select party"/><datalist id="ob-parties">{parties.map(p=><option key={p.partyId} value={p.name}/>)}</datalist></div>
          <div><label className="mb-1 block text-xs font-medium text-zinc-600">Balance (₹)</label><Input type="number" value={obAmount} onChange={e=>setObAmount(e.target.value)} placeholder="0.00"/></div>
          <Button onClick={saveOpeningBalance}>Set Balance</Button>
        </div>
      </CardContent></Card></motion.div>

      {/* Party List */}
      <motion.div variants={itemVariants}><Table><TableHeader><TableRow><TableHead>Name</TableHead><TableHead>Type</TableHead><TableHead>Credit</TableHead></TableRow></TableHeader><TableBody>
        {parties.length===0 ? <TableRow><TableCell colSpan={3} className="h-20 text-center text-sm text-zinc-500">No parties</TableCell></TableRow> : parties.map(p=><TableRow key={p.partyId}><TableCell className="font-medium">{p.name}</TableCell><TableCell><Badge>{p.type}</Badge></TableCell><TableCell>{p.creditAllowed?"Yes":"—"}</TableCell></TableRow>)}
      </TableBody></Table></motion.div>
    </motion.div>
  );
}
