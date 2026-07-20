"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { useApp } from "@/lib/app-context";
import { Button } from "@/components/ui/button";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { RefreshCw } from "lucide-react";

interface LogEntry { id: number; timestamp: string; username: string; action: string; details: string | null; }
const fadeUp = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } } };

export default function AuditPage() {
  const { companyId } = useApp();
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const [loading, setLoading] = useState(true);

  const load = () => {
    setLoading(true);
    fetch(`/api/audit?companyId=${companyId}`).then(r => r.json()).then(d => setLogs(d.logs || [])).catch(() => {}).finally(() => setLoading(false));
  };
  useEffect(() => { load(); }, [companyId]);

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div><h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Audit Logs</h1><p className="mt-0.5 text-sm text-zinc-500">{logs.length} records</p></div>
        <Button variant="outline" size="sm" onClick={load}><RefreshCw className="mr-1.5 h-3.5 w-3.5" /> Refresh</Button>
      </motion.div>

      <motion.div {...fadeUp}>
        <Table>
          <TableHeader><TableRow><TableHead>Time</TableHead><TableHead>User</TableHead><TableHead>Action</TableHead><TableHead>Details</TableHead></TableRow></TableHeader>
          <TableBody>
            {logs.length === 0 ? (
              <TableRow><TableCell colSpan={4} className="h-32 text-center"><p className="text-sm text-zinc-500">{loading ? "Loading..." : "No audit logs"}</p></TableCell></TableRow>
            ) : logs.map((l) => (
              <TableRow key={l.id}>
                <TableCell className="text-xs">{new Date(l.timestamp).toLocaleString("en-IN")}</TableCell>
                <TableCell className="font-medium">{l.username}</TableCell>
                <TableCell>{l.action}</TableCell>
                <TableCell className="max-w-xs truncate text-zinc-500">{l.details || "—"}</TableCell>
              </TableRow>
            ))}
          </TableBody>
        </Table>
      </motion.div>
    </motion.div>
  );
}
