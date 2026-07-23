"use client";

import { useState } from "react";
import { motion } from "framer-motion";
import { toast } from "sonner";
import { useApp } from "@/lib/app-context";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Select } from "@/components/ui/select";
import { Badge } from "@/components/ui/badge";
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from "@/components/ui/data-table";
import { AlertTriangle, CheckCircle, ArrowRight, Lock } from "lucide-react";
import { springs } from "@/lib/design-tokens";

interface CloseResult {
  closingFY: string;
  nextFY: string;
  partiesProcessed: number;
  balancesCarriedForward: number;
  totalDebit: number;
  totalCredit: number;
  results: { party: string; balance: number }[];
}

const itemVariants = { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0, transition: springs.default } };
function fmt(n: number) { return new Intl.NumberFormat("en-IN", { minimumFractionDigits: 2 }).format(n); }

export default function YearEndPage() {
  const { companyId, financialYear } = useApp();
  const [selectedFY, setSelectedFY] = useState(financialYear);
  const [step, setStep] = useState<"select" | "confirm" | "done">("select");
  const [processing, setProcessing] = useState(false);
  const [result, setResult] = useState<CloseResult | null>(null);

  const fyOptions = Array.from({ length: 5 }, (_, i) => {
    const year = new Date().getFullYear() - i;
    return `${year}-${year + 1}`;
  });

  const handleClose = async () => {
    setProcessing(true);
    try {
      const res = await fetch("/api/year-end", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ companyId, closingFY: selectedFY }),
      });
      const data = await res.json();
      if (res.ok) {
        setResult(data);
        setStep("done");
        toast.success(`FY ${selectedFY} closed successfully`);
      } else {
        toast.error(data.error || "Year-end close failed");
      }
    } catch { toast.error("Network error"); }
    setProcessing(false);
  };

  return (
    <motion.div initial="initial" animate="animate" className="space-y-6 max-w-3xl mx-auto">
      <motion.div variants={itemVariants}>
        <h1 className="text-2xl font-semibold text-zinc-900 dark:text-zinc-50">Year-End Close</h1>
        <p className="mt-1 text-sm text-zinc-600 dark:text-zinc-300">Carry forward party balances to the next financial year</p>
      </motion.div>

      {/* ═══ Step 1: Select FY ═══ */}
      {step === "select" && (
        <motion.div variants={itemVariants}>
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2"><Lock className="h-4 w-4" /> Select Financial Year to Close</CardTitle>
            </CardHeader>
            <CardContent className="space-y-4">
              <div>
                <label className="mb-1.5 block text-[11px] font-medium uppercase tracking-wider text-zinc-600 dark:text-zinc-300">Closing Financial Year</label>
                <Select value={selectedFY} onChange={e => setSelectedFY(e.target.value)} className="w-60">
                  {fyOptions.map(fy => <option key={fy} value={fy}>FY {fy}</option>)}
                </Select>
              </div>

              <div className="rounded-xl bg-amber-50 dark:bg-amber-900/10 border border-amber-200/50 dark:border-amber-800/30 p-4">
                <div className="flex items-start gap-3">
                  <AlertTriangle className="h-5 w-5 text-amber-600 flex-shrink-0 mt-0.5" />
                  <div>
                    <p className="text-sm font-medium text-amber-800 dark:text-amber-200">Important</p>
                    <ul className="mt-1.5 space-y-1 text-xs text-amber-700 dark:text-amber-300">
                      <li>• This will calculate the closing balance for every party in FY {selectedFY}</li>
                      <li>• Balances will be set as opening balances for FY {parseInt(selectedFY.split("-")[1])}-{parseInt(selectedFY.split("-")[1]) + 1}</li>
                      <li>• This action is safe to run multiple times (idempotent)</li>
                      <li>• No transactions will be modified or deleted</li>
                    </ul>
                  </div>
                </div>
              </div>

              <Button onClick={() => setStep("confirm")} className="w-full sm:w-auto">
                <ArrowRight className="mr-1.5 h-3.5 w-3.5" /> Preview & Confirm
              </Button>
            </CardContent>
          </Card>
        </motion.div>
      )}

      {/* ═══ Step 2: Confirm ═══ */}
      {step === "confirm" && (
        <motion.div variants={itemVariants}>
          <Card>
            <CardHeader>
              <CardTitle>Confirm Year-End Close</CardTitle>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="flex items-center gap-3 text-sm">
                <Badge variant="warning" className="text-sm px-3 py-1">FY {selectedFY}</Badge>
                <ArrowRight className="h-4 w-4 text-zinc-400" />
                <Badge variant="success" className="text-sm px-3 py-1">FY {parseInt(selectedFY.split("-")[1])}-{parseInt(selectedFY.split("-")[1]) + 1}</Badge>
              </div>
              <p className="text-sm text-zinc-600 dark:text-zinc-300">
                All party balances from FY {selectedFY} will be carried forward as opening balances to the next year.
              </p>
              <div className="flex gap-3">
                <Button onClick={handleClose} disabled={processing}>
                  {processing ? "Processing..." : "Confirm & Close Year"}
                </Button>
                <Button variant="outline" onClick={() => setStep("select")} disabled={processing}>Back</Button>
              </div>
            </CardContent>
          </Card>
        </motion.div>
      )}

      {/* ═══ Step 3: Results ═══ */}
      {step === "done" && result && (
        <motion.div variants={itemVariants} className="space-y-4">
          <Card className="p-6">
            <div className="flex items-start gap-4">
              <div className="flex h-11 w-11 items-center justify-center rounded-xl bg-emerald-100 dark:bg-emerald-900/30">
                <CheckCircle className="h-6 w-6 text-emerald-600" />
              </div>
              <div>
                <p className="text-base font-semibold text-zinc-900 dark:text-zinc-50">Year-End Close Complete</p>
                <p className="text-sm text-zinc-600 dark:text-zinc-300">FY {result.closingFY} → {result.nextFY}</p>
              </div>
            </div>
            <div className="mt-5 grid grid-cols-2 gap-3 sm:grid-cols-4">
              <div className="rounded-lg bg-zinc-50 dark:bg-zinc-800/50 p-3 text-center">
                <p className="text-xl font-bold tabular-nums text-zinc-900 dark:text-zinc-50">{result.partiesProcessed}</p>
                <p className="text-[11px] font-medium text-zinc-500">Parties Processed</p>
              </div>
              <div className="rounded-lg bg-zinc-50 dark:bg-zinc-800/50 p-3 text-center">
                <p className="text-xl font-bold tabular-nums text-blue-600">{result.balancesCarriedForward}</p>
                <p className="text-[11px] font-medium text-zinc-500">Balances Carried</p>
              </div>
              <div className="rounded-lg bg-zinc-50 dark:bg-zinc-800/50 p-3 text-center">
                <p className="text-xl font-bold tabular-nums text-red-600">₹{fmt(result.totalDebit)}</p>
                <p className="text-[11px] font-medium text-zinc-500">Total Debit</p>
              </div>
              <div className="rounded-lg bg-zinc-50 dark:bg-zinc-800/50 p-3 text-center">
                <p className="text-xl font-bold tabular-nums text-emerald-600">₹{fmt(result.totalCredit)}</p>
                <p className="text-[11px] font-medium text-zinc-500">Total Credit</p>
              </div>
            </div>
          </Card>

          {result.results.length > 0 && (
            <Card>
              <CardHeader><CardTitle>Balances Carried Forward ({result.results.length})</CardTitle></CardHeader>
              <CardContent>
                <div className="max-h-[400px] overflow-auto">
                  <Table>
                    <TableHeader><TableRow><TableHead>#</TableHead><TableHead>Party</TableHead><TableHead className="text-right">Balance</TableHead><TableHead>Type</TableHead></TableRow></TableHeader>
                    <TableBody>
                      {result.results.map((r, i) => (
                        <TableRow key={i}>
                          <TableCell className="text-xs text-zinc-500 tabular-nums">{i + 1}</TableCell>
                          <TableCell className="font-medium">{r.party}</TableCell>
                          <TableCell className={`text-right font-mono font-semibold tabular-nums ${r.balance >= 0 ? "text-red-600" : "text-emerald-600"}`}>
                            ₹{fmt(Math.abs(r.balance))}
                          </TableCell>
                          <TableCell><Badge variant={r.balance >= 0 ? "danger" : "success"}>{r.balance >= 0 ? "Receivable" : "Payable"}</Badge></TableCell>
                        </TableRow>
                      ))}
                    </TableBody>
                  </Table>
                </div>
              </CardContent>
            </Card>
          )}

          <Button variant="outline" onClick={() => { setStep("select"); setResult(null); }}>Close Another Year</Button>
        </motion.div>
      )}
    </motion.div>
  );
}
