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
import { Info, Palette, Shield, Users, Key, Moon, Sun } from "lucide-react";
import { springs } from "@/lib/design-tokens";

interface UserEntry { username: string; role: string; }
const itemVariants = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: springs.default } };

export default function SettingsPage() {
  const { companyId } = useApp();
  const [users, setUsers] = useState<UserEntry[]>([]);
  const [newUser, setNewUser] = useState("");
  const [newPass, setNewPass] = useState("");
  const [newRole, setNewRole] = useState("accounts");
  const [pwdUser, setPwdUser] = useState("");
  const [pwdNew, setPwdNew] = useState("");
  const [theme, setTheme] = useState("light");

  useEffect(() => {
    fetch("/api/users").then(r=>r.json()).then(d=>setUsers(d.users||[])).catch(()=>{});
    const stored = localStorage.getItem("mfinlogs_theme") || "light";
    setTheme(stored);
    document.documentElement.classList.toggle("dark", stored === "dark");
  }, []);

  const toggleTheme = (t: string) => {
    setTheme(t);
    localStorage.setItem("mfinlogs_theme", t);
    document.documentElement.classList.toggle("dark", t === "dark");
    toast.success(`Theme: ${t}`);
  };

  const createUser = async () => {
    if (!newUser || !newPass) { toast.error("Username and password required"); return; }
    try {
      const res = await fetch("/api/users", { method: "POST", headers: {"Content-Type":"application/json"}, body: JSON.stringify({ username: newUser, password: newPass, role: newRole }) });
      if (res.ok) { toast.success(`User "${newUser}" created`); setNewUser(""); setNewPass(""); fetch("/api/users").then(r=>r.json()).then(d=>setUsers(d.users||[])); }
      else { const e = await res.json(); toast.error(e.error||"Failed"); }
    } catch { toast.error("Network error"); }
  };

  const changePassword = async () => {
    if (!pwdUser || !pwdNew) { toast.error("Username and new password required"); return; }
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
      <motion.div variants={itemVariants}><Card><CardHeader><CardTitle className="flex items-center gap-2"><Info className="h-4 w-4"/>About</CardTitle></CardHeader><CardContent><div className="flex items-center gap-3"><div className="flex h-10 w-10 items-center justify-center rounded-xl bg-gradient-to-br from-zinc-900 to-zinc-700 dark:from-white dark:to-zinc-200"><span className="text-sm font-bold text-white dark:text-zinc-900">M</span></div><div><p className="text-sm font-medium text-zinc-900 dark:text-zinc-100">M-Finlogs Web</p><p className="text-[11px] text-zinc-500">by <span className="font-semibold">EKTHAR</span></p></div><Badge className="ml-auto">v2.0</Badge></div></CardContent></Card></motion.div>

      {/* Theme */}
      <motion.div variants={itemVariants}><Card><CardHeader><CardTitle className="flex items-center gap-2"><Palette className="h-4 w-4"/>Theme</CardTitle></CardHeader><CardContent><div className="flex gap-3">
        <button onClick={()=>toggleTheme("light")} className={`flex items-center gap-2 rounded-xl px-4 py-3 border transition-all ${theme==="light"?"border-blue-500 bg-blue-50 dark:bg-blue-900/20":"border-zinc-200 dark:border-zinc-700 hover:border-zinc-300"}`}><Sun className="h-4 w-4"/><span className="text-sm font-medium">Light</span></button>
        <button onClick={()=>toggleTheme("dark")} className={`flex items-center gap-2 rounded-xl px-4 py-3 border transition-all ${theme==="dark"?"border-blue-500 bg-blue-50 dark:bg-blue-900/20":"border-zinc-200 dark:border-zinc-700 hover:border-zinc-300"}`}><Moon className="h-4 w-4"/><span className="text-sm font-medium">Dark</span></button>
      </div></CardContent></Card></motion.div>

      {/* Admin */}
      <motion.div variants={itemVariants} className="border-t-2 border-dashed border-zinc-200 pt-6 dark:border-zinc-700">
        <h2 className="text-lg font-semibold text-zinc-900 dark:text-zinc-100 flex items-center gap-2 mb-4"><Shield className="h-5 w-5 text-blue-600"/>Admin Panel</h2>

        {/* Users */}
        <Card className="mb-4"><CardHeader><CardTitle className="flex items-center gap-2 text-base"><Users className="h-4 w-4"/>Users</CardTitle></CardHeader><CardContent className="space-y-4">
          <div className="grid grid-cols-1 gap-3 sm:grid-cols-4 items-end"><Input placeholder="Username" value={newUser} onChange={e=>setNewUser(e.target.value)}/><Input type="password" placeholder="Password" value={newPass} onChange={e=>setNewPass(e.target.value)}/><Select value={newRole} onChange={e=>setNewRole(e.target.value)}><option value="accounts">Accounts</option><option value="admin">Admin</option></Select><Button onClick={createUser}>Add User</Button></div>
          <Table><TableHeader><TableRow><TableHead>Username</TableHead><TableHead>Role</TableHead></TableRow></TableHeader><TableBody>{users.map(u=><TableRow key={u.username}><TableCell className="font-medium">{u.username}</TableCell><TableCell><Badge variant={u.role==="admin"?"info":"default"}>{u.role}</Badge></TableCell></TableRow>)}</TableBody></Table>
        </CardContent></Card>

        {/* Password */}
        <Card><CardHeader><CardTitle className="flex items-center gap-2 text-base"><Key className="h-4 w-4"/>Change Password</CardTitle></CardHeader><CardContent>
          <div className="grid grid-cols-1 gap-3 sm:grid-cols-3 items-end"><Input placeholder="Username" value={pwdUser} onChange={e=>setPwdUser(e.target.value)}/><Input type="password" placeholder="New Password" value={pwdNew} onChange={e=>setPwdNew(e.target.value)}/><Button onClick={changePassword}>Update</Button></div>
        </CardContent></Card>
      </motion.div>
    </motion.div>
  );
}
