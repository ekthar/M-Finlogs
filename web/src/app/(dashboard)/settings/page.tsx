"use client";

import { useState, useEffect } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { themes, applyTheme, getStoredTheme } from "@/lib/themes";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Select } from "@/components/ui/select";
import { Badge } from "@/components/ui/badge";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { Info, Palette, Shield, Users, Key, Lock, Download, DollarSign, Calendar } from "lucide-react";
import { springs } from "@/lib/design-tokens";

interface UserEntry { username: string; role: string; }
const itemVariants = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: springs.default } };

export default function SettingsPage() {
  const { companyId, role } = useApp();
  const [users, setUsers] = useState<UserEntry[]>([]);
  const [newUser, setNewUser] = useState("");
  const [newPass, setNewPass] = useState("");
  const [newRole, setNewRole] = useState("accounts");
  const [pwdUser, setPwdUser] = useState("");
  const [pwdNew, setPwdNew] = useState("");
  const [activeTheme, setActiveTheme] = useState("light");
  const [lockDate, setLockDate] = useState("");
  const [newLockDate, setNewLockDate] = useState("");
  const [cashDate, setCashDate] = useState(new Date().toISOString().split("T")[0]);
  const [cashAmount, setCashAmount] = useState("");
  const [brandName, setBrandName] = useState("M-Finlogs");
  const [brandCompany, setBrandCompany] = useState("");
  const [brandTagline, setBrandTagline] = useState("");

  useEffect(() => {
    setActiveTheme(getStoredTheme());
    // Load branding
    fetch(`/api/settings/branding?companyId=${companyId}`).then(r=>r.json()).then(d => {
      if (d.softwareName) setBrandName(d.softwareName);
      if (d.companyDisplayName) setBrandCompany(d.companyDisplayName);
      if (d.companyTagline) setBrandTagline(d.companyTagline);
    }).catch(()=>{});
    if (role === "admin") {
      fetch("/api/users").then(r=>r.json()).then(d=>setUsers(d.users||[])).catch(()=>{});
      fetch(`/api/day-lock?companyId=${companyId}`).then(r=>r.json()).then(d=>setLockDate(d.lockDate||"")).catch(()=>{});
    }
  }, [role, companyId]);

  const saveBranding = async () => {
    const res = await fetch("/api/settings/branding", { method: "POST", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ companyId, softwareName: brandName, companyDisplayName: brandCompany, companyTagline: brandTagline }) });
    if (res.ok) toast.success("Branding saved"); else toast.error("Failed");
  };

  const handleTheme = (key: string) => { applyTheme(key); setActiveTheme(key); toast.success(`Theme: ${themes.find(t=>t.key===key)?.name}`); };

  const createUser = async () => {
    if (!newUser || !newPass) { toast.error("Username and password required"); return; }
    if (newPass.length < 6) { toast.error("Password min 6 chars"); return; }
    const res = await fetch("/api/users", { method: "POST", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ username: newUser, password: newPass, role: newRole }) });
    if (res.ok) { toast.success(`User "${newUser}" created`); setNewUser(""); setNewPass(""); fetch("/api/users").then(r=>r.json()).then(d=>setUsers(d.users||[])); }
    else { const e = await res.json(); toast.error(e.error||"Failed"); }
  };

  const changePassword = async () => {
    if (!pwdUser || !pwdNew || pwdNew.length < 6) { toast.error("Username + password (min 6)"); return; }
    const res = await fetch("/api/users/password", { method: "PUT", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ username: pwdUser, newPassword: pwdNew }) });
    if (res.ok) { toast.success("Password updated"); setPwdUser(""); setPwdNew(""); } else { const e = await res.json(); toast.error(e.error||"Failed"); }
  };

  const setDayLock = async () => {
    if (!newLockDate) { toast.error("Select a date"); return; }
    const res = await fetch("/api/day-lock", { method: "POST", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ companyId, lockDate: newLockDate }) });
    if (res.ok) { setLockDate(newLockDate); toast.success(`Day locked up to ${newLockDate}`); } else toast.error("Failed");
  };

  const saveCashInHand = async () => {
    if (!cashDate || !cashAmount) { toast.error("Date and amount required"); return; }
    const res = await fetch("/api/cash-in-hand", { method: "POST", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ companyId, date: cashDate, cashInHand: parseFloat(cashAmount) }) });
    if (res.ok) { toast.success(`Cash In Hand saved for ${cashDate}`); setCashAmount(""); } else toast.error("Failed");
  };

  const downloadExport = () => { window.open(`/api/export?companyId=${companyId}`, "_blank"); toast.success("Downloading..."); };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-6 max-w-3xl">
      <motion.div variants={itemVariants}><h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-100">Settings</h1></motion.div>

      {/* About + Branding */}
      <motion.div variants={itemVariants}><Card><CardHeader><CardTitle className="flex items-center gap-2"><Info className="h-4 w-4"/>Branding</CardTitle></CardHeader><CardContent className="space-y-4">
        <div className="flex items-center gap-3 mb-4"><div className="flex h-10 w-10 items-center justify-center rounded-xl bg-gradient-to-br from-zinc-900 to-zinc-700 dark:from-white dark:to-zinc-200"><span className="text-sm font-bold text-white dark:text-zinc-900">M</span></div><div><p className="text-sm font-medium text-zinc-900 dark:text-zinc-100">M-Finlogs Web</p><p className="text-[11px] text-zinc-500">by EKTHAR</p></div><Badge className="ml-auto">v2.2</Badge></div>
        <div><label className="mb-1.5 block text-xs font-medium text-zinc-600">Software Name</label><Input placeholder="M-Finlogs" value={brandName} onChange={e => setBrandName(e.target.value)}/></div>
        <div><label className="mb-1.5 block text-xs font-medium text-zinc-600">Company Display Name</label><Input placeholder="Your Company Name" value={brandCompany} onChange={e => setBrandCompany(e.target.value)}/></div>
        <div><label className="mb-1.5 block text-xs font-medium text-zinc-600">Tagline (optional)</label><Input placeholder="e.g. Since 1998" value={brandTagline} onChange={e => setBrandTagline(e.target.value)}/></div>
        <Button onClick={saveBranding} variant="outline" className="w-full">Save Branding</Button>
      </CardContent></Card></motion.div>

      {/* Theme */}
      <motion.div variants={itemVariants}><Card><CardHeader><CardTitle className="flex items-center gap-2"><Palette className="h-4 w-4"/>Theme</CardTitle></CardHeader><CardContent><div className="grid grid-cols-2 gap-3 sm:grid-cols-4">{themes.map(t=>(<button key={t.key} onClick={()=>handleTheme(t.key)} className={`group relative rounded-xl p-3 border-2 transition-all text-left ${activeTheme===t.key?"border-blue-500 ring-2 ring-blue-500/20":"border-zinc-200 dark:border-zinc-700 hover:border-zinc-300"}`}><div className="flex gap-1.5 mb-2"><div className="h-5 w-5 rounded-md" style={{background:t.preview.bg}}/><div className="h-5 w-5 rounded-md border" style={{background:t.preview.card}}/><div className="h-5 w-5 rounded-md" style={{background:t.preview.accent}}/></div><p className="text-xs font-medium text-zinc-900 dark:text-zinc-100">{t.name}</p><p className="text-[10px] text-zinc-500 mt-0.5">{t.description}</p>{activeTheme===t.key&&<div className="absolute top-2 right-2 h-2 w-2 rounded-full bg-blue-500"/>}</button>))}</div></CardContent></Card></motion.div>

      {/* Admin Panel */}
      {role === "admin" && (<motion.div variants={itemVariants} className="border-t-2 border-dashed border-zinc-200 pt-6 dark:border-zinc-700">
        <h2 className="text-lg font-semibold text-zinc-900 dark:text-zinc-100 flex items-center gap-2 mb-4"><Shield className="h-5 w-5 text-blue-600"/>Admin Panel</h2>

        {/* Cash In Hand */}
        <Card className="mb-4"><CardHeader><CardTitle className="flex items-center gap-2 text-base"><DollarSign className="h-4 w-4"/>Cash In Hand</CardTitle></CardHeader><CardContent>
          <p className="text-xs text-zinc-500 mb-3">Record actual closing cash for a date (used by Daily Summary Short/Excess)</p>
          <div className="grid grid-cols-1 gap-3 sm:grid-cols-3 items-end"><Input type="date" value={cashDate} onChange={e=>setCashDate(e.target.value)}/><Input type="number" placeholder="Amount" value={cashAmount} onChange={e=>setCashAmount(e.target.value)}/><Button onClick={saveCashInHand}>Save</Button></div>
        </CardContent></Card>

        {/* Day Lock */}
        <Card className="mb-4"><CardHeader><CardTitle className="flex items-center gap-2 text-base"><Lock className="h-4 w-4"/>Day Lock</CardTitle></CardHeader><CardContent>
          <p className="text-xs text-zinc-500 mb-3">Lock transactions on/before a date (prevents edits to closed books)</p>
          {lockDate && <p className="text-xs mb-2">Currently locked up to: <Badge variant="warning">{lockDate}</Badge></p>}
          <div className="grid grid-cols-1 gap-3 sm:grid-cols-2 items-end"><Input type="date" value={newLockDate} onChange={e=>setNewLockDate(e.target.value)}/><Button onClick={setDayLock}>Lock Up To Date</Button></div>
        </CardContent></Card>

        {/* Data Export */}
        <Card className="mb-4"><CardHeader><CardTitle className="flex items-center gap-2 text-base"><Download className="h-4 w-4"/>Data Export</CardTitle></CardHeader><CardContent>
          <p className="text-xs text-zinc-500 mb-3">Download all data (transactions, parties, inventory, settings) as JSON</p>
          <Button variant="outline" onClick={downloadExport}><Download className="mr-1.5 h-3.5 w-3.5"/>Download Full Backup</Button>
        </CardContent></Card>

        {/* Users */}
        <Card className="mb-4"><CardHeader><CardTitle className="flex items-center gap-2 text-base"><Users className="h-4 w-4"/>Users</CardTitle></CardHeader><CardContent className="space-y-4">
          <div className="grid grid-cols-1 gap-3 sm:grid-cols-4 items-end"><Input placeholder="Username" value={newUser} onChange={e=>setNewUser(e.target.value)}/><Input type="password" placeholder="Password" value={newPass} onChange={e=>setNewPass(e.target.value)}/><Select value={newRole} onChange={e=>setNewRole(e.target.value)}><option value="accounts">Accounts</option><option value="admin">Admin</option></Select><Button onClick={createUser}>Add</Button></div>
          <Table><TableHeader><TableRow><TableHead>User</TableHead><TableHead>Role</TableHead></TableRow></TableHeader><TableBody>{users.map(u=><TableRow key={u.username}><TableCell className="font-medium">{u.username}</TableCell><TableCell><Badge variant={u.role==="admin"?"info":"default"}>{u.role}</Badge></TableCell></TableRow>)}</TableBody></Table>
        </CardContent></Card>

        {/* Password */}
        <Card><CardHeader><CardTitle className="flex items-center gap-2 text-base"><Key className="h-4 w-4"/>Change Password</CardTitle></CardHeader><CardContent>
          <div className="grid grid-cols-1 gap-3 sm:grid-cols-3 items-end"><Input placeholder="Username" value={pwdUser} onChange={e=>setPwdUser(e.target.value)}/><Input type="password" placeholder="New Password" value={pwdNew} onChange={e=>setPwdNew(e.target.value)}/><Button onClick={changePassword}>Update</Button></div>
        </CardContent></Card>
      </motion.div>)}
    </motion.div>
  );
}
