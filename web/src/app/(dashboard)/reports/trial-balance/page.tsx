"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { Button } from "@/components/ui/button";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { Printer } from "lucide-react";

interface Account { name: string; type: string; debit: number; credit: number; }
const fadeUp = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: { type: "spring" as const, bounce: 0, duration: 0.4 } } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }

export default function TrialBalancePage() {
  const [accounts, setAccounts] = useState<Account[]>([]);
  const [totalDebit, setTotalDebit] = useState(0);
  const [totalCredit, setTotalCredit] = useState(0);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    fetch("/api/reports/trial-balance").then(r => r.json()).then(d => {
      setAccounts(d.accounts || []);
      setTotalDebit(d.totalDebit || 0);
      setTotalCredit(d.totalCredit || 0);
    }).catch(() => {}).finally(() => setLoading(false));
  }, []);

  return (
    <motion.div initial="initial" animate="animate" className="space-y-5">
      <motion.div {...fadeUp} className="flex items-center justify-between">
        <div><h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Trial Balance</h1><p className="mt-0.5 text-sm text-zinc-500">Debit and credit totals per account</p></div>
        <Button variant="outline" size="sm" onClick={() => window.print()}><Printer className="mr-1.5 h-3.5 w-3.5" /> Print</Button>
      </motion.div>

      <motion.div {...fadeUp}>
        <Table>
          <TableHeader><TableRow><TableHead>Account / Party</TableHead><TableHead className="text-right">Debit</TableHead><TableHead className="text-right">Credit</TableHead></TableRow></TableHeader>
          <TableBody>
            {accounts.length === 0 ? (
              <TableRow><TableCell colSpan={3} className="h-32 text-center"><p className="text-sm text-zinc-500">{loading ? "Loading..." : "No data"}</p></TableCell></TableRow>
            ) : (
              <>
                {accounts.map((a, i) => (
                  <TableRow key={i}><TableCell className="font-medium">{a.name}</TableCell><TableCell className="text-right font-mono">{a.debit > 0 ? fmt(a.debit) : ""}</TableCell><TableCell className="text-right font-mono">{a.credit > 0 ? fmt(a.credit) : ""}</TableCell></TableRow>
                ))}
                <TableRow className="border-t-2 font-semibold"><TableCell>Total</TableCell><TableCell className="text-right font-mono">{fmt(totalDebit)}</TableCell><TableCell className="text-right font-mono">{fmt(totalCredit)}</TableCell></TableRow>
              </>
            )}
          </TableBody>
        </Table>
      </motion.div>
    </motion.div>
  );
}
