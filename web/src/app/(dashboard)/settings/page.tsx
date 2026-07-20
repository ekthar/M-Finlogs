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
import { Info, Palette, Shield, Users, Key, Lock, Download, DollarSign, GitBranch, Trash2, Plus } from "lucide-react";
import { springs } from "@/lib/design-tokens";

interface UserEntry { username: string; role: string; }
interface Branch { id: string; name: string; location: string; }
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
  const [branches, setBranches] = useState<Branch[]>([]);
  const [newBranchName, setNewBranchName] = useState("");
  const [newBranchLocation, setNewBranchLocation] = useState("");

  useEffect(() => {
    setActiveTheme(getStoredTheme());
    fetch(`/api/settings/branding?companyId=${companyId}`).then(r=>r.json()).then(d => {
      if (d.softwareName) setBrandName(d.softwareName);
      if (d.companyDisplayName) setBrandCompany(d.companyDisplayName);
      if (d.companyTagline) setBrandTagline(d.companyTagline);
    }).catch(()=>{});
    fetch(`/api/branches?companyId=${companyId}`).then(r=>r.json()).then(d=>setBranches(d.branches||[])).catch(()=>{});
    if (role === "admin") {
      fetch("/api/users").then(r=>r.json()).then(d=>setUsers(d.users||[])).catch(()=>{});
      fetch(`/api/day-lock?companyId=${companyId}`).then(r=>r.json()).then(d=>setLockDate(d.lockDate||"")).catch(()=>{});
    }
  }, [role, companyId]);

  const handleTheme = (key: string) => { applyTheme(key); setActiveTheme(key); toast.success(`Theme: ${themes.find(t=>t.key===key)?.name}`); };
  const saveBranding = async () => { const res = await fetch("/api/settings/branding", { method: "POST", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ companyId, softwareName: brandName, companyDisplayName: brandCompany, companyTagline: brandTagline }) }); if (res.ok) toast.success("Branding saved"); else toast.error("Failed"); };

  const createUser = async () => {
    if (!newUser || !newPass || newPass.length < 6) { toast.error("Username + password (min 6 chars)"); return; }
    const res = await fetch("/api/users", { method: "POST", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ username: newUser, password: newPass, role: newRole }) });
    if (res.ok) { toast.success(`User "${newUser}" created`); setNewUser(""); setNewPass(""); fetch("/api/users").then(r=>r.json()).then(d=>setUsers(d.users||[])); }
    else { const e = await res.json(); toast.error(e.error||"Failed"); }
  };

  const deactivateUser = async (username: string) => {
    if (!confirm(`Deactivate "${username}"? They won't be able to login.`)) return;
    const res = await fetch(`/api/users/${username}`, { method: "DELETE" });
    if (res.ok) { toast.success(`${username} deactivated`); fetch("/api/users").then(r=>r.json()).then(d=>setUsers(d.users||[])); }
    else { const e = await res.json(); toast.error(e.error||"Failed"); }
  };

  const changePassword = async () => {
    if (!pwdUser || !pwdNew || pwdNew.length < 6) { toast.error("Username + password (min 6)"); return; }
    const res = await fetch("/api/users/password", { method: "PUT", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ username: pwdUser, newPassword: pwdNew }) });
    if (res.ok) { toast.success("Password updated"); setPwdUser(""); setPwdNew(""); } else { const e = await res.json(); toast.error(e.error||"Failed"); }
  };

  const setDayLock = async () => { if (!newLockDate) return; const res = await fetch("/api/day-lock", { method: "POST", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ companyId, lockDate: newLockDate }) }); if (res.ok) { setLockDate(newLockDate); toast.success(`Locked up to ${newLockDate}`); } else toast.error("Failed"); };
  const saveCashInHand = async () => { if (!cashDate||!cashAmount) { toast.error("Date + amount"); return; } const res = await fetch("/api/cash-in-hand", { method: "POST", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ companyId, date: cashDate, cashInHand: parseFloat(cashAmount) }) }); if (res.ok) { toast.success("Saved"); setCashAmount(""); } else toast.error("Failed"); };
  const downloadExport = () => { window.open(`/api/export?companyId=${companyId}`, "_blank"); toast.success("Downloading..."); };

  const addBranch = async () => {
    if (!newBranchName) { toast.error("Branch name required"); return; }
    const newBranch = { id: `branch_${Date.now()}`, name: newBranchName, location: newBranchLocation };
    const updated = [...branches, newBranch];
    const res = await fetch("/api/branches", { method: "POST", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ companyId, branches: updated }) });
    if (res.ok) { setBranches(updated); setNewBranchName(""); setNewBranchLocation(""); toast.success("Branch added"); } else toast.error("Failed");
  };

  const removeBranch = async (id: string) => {
    if (id === "main") { toast.error("Cannot remove main branch"); return; }
    const updated = branches.filter(b => b.id !== id);
    await fetch("/api/branches", { method: "POST", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ companyId, branches: updated }) });
    setBranches(updated); toast.success("Branch removed");
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-6 max-w-3xl">
      <motion.div variants={itemVariants}><h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-100">Settings</h1></motion.div>

      {/* Branding */}
      <motion.div variants={itemVariants}><Card><CardHeader><CardTitle className="flex items-center gap-2"><Info className="h-4 w-4"/>Branding</CardTitle></CardHeader><CardContent className="space-y-3">
        <div className="flex items-center gap-3 pb-3 border-b border-zinc-100 dark:border-zinc-800"><div className="flex h-10 w-10 items-center justify-center rounded-xl bg-gradient-to-br from-zinc-900 to-zinc-700 dark:from-white dark:to-zinc-200"><span className="text-sm font-bold text-white dark:text-zinc-900">M</span></div><div><p className="text-sm font-medium text-zinc-900 dark:text-zinc-100">{brandName}</p><p className="text-[11px] text-zinc-500">{brandCompany || "Your Company"} {brandTagline && `• ${brandTagline}`}</p></div><Badge className="ml-auto">v2.3</Badge></div>
        <div className="grid grid-cols-1 gap-3 sm:grid-cols-3">
          <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Software Name</label><Input value={brandName} onChange={e=>setBrandName(e.target.value)} placeholder="M-Finlogs"/></div>
          <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Company Name</label><Input value={brandCompany} onChange={e=>setBrandCompany(e.target.value)} placeholder="Your Company"/></div>
          <div><label className="mb-1 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">Tagline</label><Input value={brandTagline} onChange={e=>setBrandTagline(e.target.value)} placeholder="Since 1998"/></div>
        </div>
        <Button variant="outline" size="sm" onClick={saveBranding}>Save Branding</Button>
      </CardContent></Card></motion.div>

      {/* Theme */}
      <motion.div variants={itemVariants}><Card><CardHeader><CardTitle className="flex items-center gap-2"><Palette className="h-4 w-4"/>Theme</CardTitle></CardHeader><CardContent><div className="grid grid-cols-2 gap-3 sm:grid-cols-4">{themes.map(t=>(<button key={t.key} onClick={()=>handleTheme(t.key)} className={`group relative rounded-xl p-3 border-2 transition-all text-left ${activeTheme===t.key?"border-blue-500 ring-2 ring-blue-500/20":"border-zinc-200 dark:border-zinc-700 hover:border-zinc-300"}`}><div className="flex gap-1.5 mb-2"><div className="h-5 w-5 rounded-md" style={{background:t.preview.bg}}/><div className="h-5 w-5 rounded-md border" style={{background:t.preview.card}}/><div className="h-5 w-5 rounded-md" style={{background:t.preview.accent}}/></div><p className="text-xs font-medium text-zinc-900 dark:text-zinc-100">{t.name}</p><p className="text-[10px] text-zinc-500 mt-0.5">{t.description}</p>{activeTheme===t.key&&<div className="absolute top-2 right-2 h-2 w-2 rounded-full bg-blue-500"/>}</button>))}</div></CardContent></Card></motion.div>

      {/* Branches */}
      <motion.div variants={itemVariants}><Card><CardHeader><CardTitle className="flex items-center gap-2"><GitBranch className="h-4 w-4"/>Branches</CardTitle></CardHeader><CardContent className="space-y-3">
        <p className="text-xs text-zinc-500">Manage multiple locations/branches. Transactions can be filtered by branch.</p>
        <Table><TableHeader><TableRow><TableHead>Branch</TableHead><TableHead>Location</TableHead><TableHead className="w-16"></TableHead></TableRow></TableHeader><TableBody>
          {branches.map(b=><TableRow key={b.id}><TableCell className="font-medium">{b.name}</TableCell><TableCell className="text-zinc-500">{b.location||"—"}</TableCell><TableCell>{b.id!=="main"&&<Button variant="ghost" size="icon" className="h-7 w-7 text-red-500" onClick={()=>removeBranch(b.id)}><Trash2 className="h-3 w-3"/></Button>}</TableCell></TableRow>)}
        </TableBody></Table>
        <div className="grid grid-cols-1 gap-2 sm:grid-cols-3 items-end">
          <Input placeholder="Branch name" value={newBranchName} onChange={e=>setNewBranchName(e.target.value)}/>
          <Input placeholder="Location (optional)" value={newBranchLocation} onChange={e=>setNewBranchLocation(e.target.value)}/>
          <Button variant="secondary" size="sm" onClick={addBranch}><Plus className="mr-1 h-3.5 w-3.5"/>Add Branch</Button>
        </div>
      </CardContent></Card></motion.div>

      {/* Admin Panel */}
      {role === "admin" && (<motion.div variants={itemVariants} className="border-t-2 border-dashed border-zinc-200 pt-6 dark:border-zinc-700">
        <h2 className="text-lg font-semibold text-zinc-900 dark:text-zinc-100 flex items-center gap-2 mb-4"><Shield className="h-5 w-5 text-blue-600"/>Admin Panel</h2>

        {/* Cash In Hand */}
        <Card className="mb-4"><CardHeader><CardTitle className="flex items-center gap-2 text-base"><DollarSign className="h-4 w-4"/>Cash In Hand</CardTitle></CardHeader><CardContent>
          <p className="text-xs text-zinc-500 mb-3">Record actual closing cash (for Daily Summary Short/Excess)</p>
          <div className="grid grid-cols-1 gap-3 sm:grid-cols-3 items-end"><Input type="date" value={cashDate} onChange={e=>setCashDate(e.target.value)}/><Input type="number" placeholder="Amount" value={cashAmount} onChange={e=>setCashAmount(e.target.value)}/><Button onClick={saveCashInHand}>Save</Button></div>
        </CardContent></Card>

        {/* Day Lock */}
        <Card className="mb-4"><CardHeader><CardTitle className="flex items-center gap-2 text-base"><Lock className="h-4 w-4"/>Day Lock</CardTitle></CardHeader><CardContent>
          <p className="text-xs text-zinc-500 mb-3">Lock transactions on/before a date (prevents edits)</p>
          {lockDate && <p className="text-xs mb-2">Locked up to: <Badge variant="warning">{lockDate}</Badge></p>}
          <div className="grid grid-cols-1 gap-3 sm:grid-cols-2 items-end"><Input type="date" value={newLockDate} onChange={e=>setNewLockDate(e.target.value)}/><Button onClick={setDayLock}>Lock</Button></div>
        </CardContent></Card>

        {/* Export */}
        <Card className="mb-4"><CardHeader><CardTitle className="flex items-center gap-2 text-base"><Download className="h-4 w-4"/>Data</CardTitle></CardHeader><CardContent>
          <Button variant="outline" onClick={downloadExport}><Download className="mr-1.5 h-3.5 w-3.5"/>Download Full Backup (JSON)</Button>
        </CardContent></Card>

        {/* User Management */}
        <Card className="mb-4"><CardHeader><CardTitle className="flex items-center gap-2 text-base"><Users className="h-4 w-4"/>User Management</CardTitle></CardHeader><CardContent className="space-y-4">
          <div className="grid grid-cols-1 gap-3 sm:grid-cols-4 items-end">
            <Input placeholder="Username" value={newUser} onChange={e=>setNewUser(e.target.value)}/>
            <Input type="password" placeholder="Password (min 6)" value={newPass} onChange={e=>setNewPass(e.target.value)}/>
            <Select value={newRole} onChange={e=>setNewRole(e.target.value)}><option value="accounts">Accounts</option><option value="admin">Admin</option></Select>
            <Button onClick={createUser}>Create User</Button>
          </div>
          <Table><TableHeader><TableRow><TableHead>User</TableHead><TableHead>Role</TableHead><TableHead className="w-24">Action</TableHead></TableRow></TableHeader><TableBody>
            {users.map(u=><TableRow key={u.username}>
              <TableCell><div className="flex items-center gap-2"><div className="flex h-7 w-7 items-center justify-center rounded-full bg-gradient-to-br from-blue-500 to-indigo-600 text-[10px] font-bold text-white">{u.username.charAt(0).toUpperCase()}</div><span className="font-medium">{u.username}</span></div></TableCell>
              <TableCell><Badge variant={u.role==="admin"?"info":"default"}>{u.role}</Badge></TableCell>
              <TableCell>{u.username!=="admin"&&<Button variant="ghost" size="sm" className="text-red-500 text-xs" onClick={()=>deactivateUser(u.username)}>Deactivate</Button>}</TableCell>
            </TableRow>)}
          </TableBody></Table>
        </CardContent></Card>

        {/* Password */}
        <Card><CardHeader><CardTitle className="flex items-center gap-2 text-base"><Key className="h-4 w-4"/>Reset Password</CardTitle></CardHeader><CardContent>
          <div className="grid grid-cols-1 gap-3 sm:grid-cols-3 items-end"><Input placeholder="Username" value={pwdUser} onChange={e=>setPwdUser(e.target.value)}/><Input type="password" placeholder="New Password (min 6)" value={pwdNew} onChange={e=>setPwdNew(e.target.value)}/><Button onClick={changePassword}>Reset</Button></div>
        </CardContent></Card>

        {/* Install App hint */}
        <Card className="mt-4"><CardContent className="py-4">
          <p className="text-xs font-medium text-zinc-700 dark:text-zinc-300 mb-1">Install as Desktop App</p>
          <p className="text-[11px] text-zinc-500">On Chrome/Edge: click the install icon in the address bar (or Menu → Install App). On mobile: Add to Home Screen. The app works offline after installation.</p>
        </CardContent></Card>
      </motion.div>)}
    </motion.div>
  );
}
