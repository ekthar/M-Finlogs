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
import { Info, Palette, Shield, Users, Key } from "lucide-react";
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

  useEffect(() => {
    setActiveTheme(getStoredTheme());
    if (role === "admin") fetch("/api/users").then(r=>r.json()).then(d=>setUsers(d.users||[])).catch(()=>{});
  }, [role]);

  const handleTheme = (key: string) => {
    applyTheme(key);
    setActiveTheme(key);
    toast.success(`Theme: ${themes.find(t=>t.key===key)?.name}`);
  };

  const createUser = async () => {
    if (!newUser || !newPass) { toast.error("Username and password required"); return; }
    if (newPass.length < 6) { toast.error("Password must be at least 6 characters"); return; }
    try {
      const res = await fetch("/api/users", { method: "POST", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ username: newUser, password: newPass, role: newRole }) });
      if (res.ok) { toast.success(`User "${newUser}" created`); setNewUser(""); setNewPass(""); fetch("/api/users").then(r=>r.json()).then(d=>setUsers(d.users||[])); }
      else { const e = await res.json(); toast.error(e.error||"Failed"); }
    } catch { toast.error("Network error"); }
  };

  const changePassword = async () => {
    if (!pwdUser || !pwdNew) { toast.error("Username and password required"); return; }
    if (pwdNew.length < 6) { toast.error("Min 6 characters"); return; }
    try {
      const res = await fetch("/api/users/password", { method: "PUT", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ username: pwdUser, newPassword: pwdNew }) });
      if (res.ok) { toast.success("Password updated"); setPwdUser(""); setPwdNew(""); }
      else { const e = await res.json(); toast.error(e.error||"Failed"); }
    } catch { toast.error("Network error"); }
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-6 max-w-3xl">
      <motion.div variants={itemVariants}><h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-100">Settings</h1><p className="mt-0.5 text-sm text-zinc-500">Preferences and administration</p></motion.div>

      {/* About */}
      <motion.div variants={itemVariants}><Card><CardHeader><CardTitle className="flex items-center gap-2"><Info className="h-4 w-4"/>About</CardTitle></CardHeader><CardContent><div className="flex items-center gap-3"><div className="flex h-10 w-10 items-center justify-center rounded-xl bg-gradient-to-br from-zinc-900 to-zinc-700 dark:from-white dark:to-zinc-200"><span className="text-sm font-bold text-white dark:text-zinc-900">M</span></div><div><p className="text-sm font-medium text-zinc-900 dark:text-zinc-100">M-Finlogs Web</p><p className="text-[11px] text-zinc-500">by <span className="font-semibold">EKTHAR</span></p></div><Badge className="ml-auto">v2.1</Badge></div></CardContent></Card></motion.div>

      {/* Theme */}
      <motion.div variants={itemVariants}><Card><CardHeader><CardTitle className="flex items-center gap-2"><Palette className="h-4 w-4"/>Theme</CardTitle></CardHeader><CardContent>
        <div className="grid grid-cols-2 gap-3 sm:grid-cols-4">
          {themes.map(t => (
            <button key={t.key} onClick={() => handleTheme(t.key)} className={`group relative rounded-xl p-3 border-2 transition-all text-left ${activeTheme===t.key ? "border-blue-500 ring-2 ring-blue-500/20" : "border-zinc-200 dark:border-zinc-700 hover:border-zinc-300 dark:hover:border-zinc-600"}`}>
              <div className="flex gap-1.5 mb-2">
                <div className="h-5 w-5 rounded-md" style={{background: t.preview.bg}}/>
                <div className="h-5 w-5 rounded-md border border-zinc-200/50" style={{background: t.preview.card}}/>
                <div className="h-5 w-5 rounded-md" style={{background: t.preview.accent}}/>
              </div>
              <p className="text-xs font-medium text-zinc-900 dark:text-zinc-100">{t.name}</p>
              <p className="text-[10px] text-zinc-500 mt-0.5">{t.description}</p>
              {activeTheme===t.key && <div className="absolute top-2 right-2 h-2 w-2 rounded-full bg-blue-500"/>}
            </button>
          ))}
        </div>
      </CardContent></Card></motion.div>

      {/* Admin Panel (only visible to admin role) */}
      {role === "admin" && (
        <motion.div variants={itemVariants} className="border-t-2 border-dashed border-zinc-200 pt-6 dark:border-zinc-700">
          <h2 className="text-lg font-semibold text-zinc-900 dark:text-zinc-100 flex items-center gap-2 mb-4"><Shield className="h-5 w-5 text-blue-600"/>Admin Panel</h2>

          <Card className="mb-4"><CardHeader><CardTitle className="flex items-center gap-2 text-base"><Users className="h-4 w-4"/>Users</CardTitle></CardHeader><CardContent className="space-y-4">
            <div className="grid grid-cols-1 gap-3 sm:grid-cols-4 items-end"><Input placeholder="Username" value={newUser} onChange={e=>setNewUser(e.target.value)}/><Input type="password" placeholder="Password (min 6)" value={newPass} onChange={e=>setNewPass(e.target.value)}/><Select value={newRole} onChange={e=>setNewRole(e.target.value)}><option value="accounts">Accounts</option><option value="admin">Admin</option></Select><Button onClick={createUser}>Add</Button></div>
            <Table><TableHeader><TableRow><TableHead>Username</TableHead><TableHead>Role</TableHead></TableRow></TableHeader><TableBody>{users.map(u=><TableRow key={u.username}><TableCell className="font-medium">{u.username}</TableCell><TableCell><Badge variant={u.role==="admin"?"info":"default"}>{u.role}</Badge></TableCell></TableRow>)}</TableBody></Table>
          </CardContent></Card>

          <Card><CardHeader><CardTitle className="flex items-center gap-2 text-base"><Key className="h-4 w-4"/>Change Password</CardTitle></CardHeader><CardContent>
            <div className="grid grid-cols-1 gap-3 sm:grid-cols-3 items-end"><Input placeholder="Username" value={pwdUser} onChange={e=>setPwdUser(e.target.value)}/><Input type="password" placeholder="New Password (min 6)" value={pwdNew} onChange={e=>setPwdNew(e.target.value)}/><Button onClick={changePassword}>Update</Button></div>
          </CardContent></Card>
        </motion.div>
      )}
    </motion.div>
  );
}
