"use client";

import { useState, useEffect, useRef } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { toast } from "sonner";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Select } from "@/components/ui/select";
import { Badge } from "@/components/ui/badge";
import { X, Save, Hash, Calendar, CreditCard, DollarSign, FileText } from "lucide-react";

interface Transaction {
  txnId: number;
  txnDate: string;
  billNo: string | null;
  txnType: string;
  paymentMode: string;
  amount: string;
  party: { name: string };
}

interface EditTransactionModalProps {
  transaction: Transaction | null;
  open: boolean;
  onClose: () => void;
  onSaved: () => void;
}

type EditField = "amount" | "bill_no" | "txn_date" | "payment_mode" | "txn_type";

const fields: { key: EditField; label: string; icon: typeof DollarSign }[] = [
  { key: "amount", label: "Amount", icon: DollarSign },
  { key: "bill_no", label: "Bill No", icon: Hash },
  { key: "txn_date", label: "Date", icon: Calendar },
  { key: "payment_mode", label: "Mode", icon: CreditCard },
  { key: "txn_type", label: "Type", icon: FileText },
];

export function EditTransactionModal({ transaction, open, onClose, onSaved }: EditTransactionModalProps) {
  const [selectedField, setSelectedField] = useState<EditField>("amount");
  const [value, setValue] = useState("");
  const [saving, setSaving] = useState(false);
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    if (open && transaction) {
      setSelectedField("amount");
      setValue(String(transaction.amount));
      setTimeout(() => inputRef.current?.focus(), 100);
    }
  }, [open, transaction]);

  useEffect(() => {
    if (!transaction) return;
    switch (selectedField) {
      case "amount": setValue(String(transaction.amount)); break;
      case "bill_no": setValue(transaction.billNo || ""); break;
      case "txn_date": setValue(new Date(transaction.txnDate).toISOString().split("T")[0]); break;
      case "payment_mode": setValue(transaction.paymentMode); break;
      case "txn_type": setValue(transaction.txnType); break;
    }
    setTimeout(() => inputRef.current?.focus(), 50);
  }, [selectedField, transaction]);

  const handleSave = async () => {
    if (!transaction) return;
    setSaving(true);
    try {
      const res = await fetch(`/api/transactions/${transaction.txnId}`, {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ field: selectedField, value }),
      });
      if (res.ok) {
        toast.success("Transaction updated", {
          description: `${fields.find(f => f.key === selectedField)?.label} changed successfully`,
        });
        onSaved();
        onClose();
      } else {
        const err = await res.json();
        toast.error(err.error || "Update failed");
      }
    } catch { toast.error("Network error"); }
    setSaving(false);
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === "Enter") handleSave();
    if (e.key === "Escape") onClose();
  };

  if (!transaction) return null;

  return (
    <AnimatePresence>
      {open && (
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 backdrop-blur-sm"
          onClick={onClose}
        >
          <motion.div
            initial={{ opacity: 0, scale: 0.92, y: 20 }}
            animate={{ opacity: 1, scale: 1, y: 0 }}
            exit={{ opacity: 0, scale: 0.92, y: 20 }}
            transition={{ type: "spring" as const, bounce: 0.1, duration: 0.35 }}
            className="w-full max-w-md rounded-2xl border border-zinc-200/80 bg-white shadow-2xl dark:border-zinc-700 dark:bg-zinc-900"
            onClick={(e) => e.stopPropagation()}
          >
            {/* Header */}
            <div className="flex items-center justify-between border-b border-zinc-100 px-5 py-4 dark:border-zinc-800">
              <div>
                <h3 className="text-base font-semibold text-zinc-900 dark:text-zinc-100">Edit Transaction</h3>
                <p className="text-xs text-zinc-500 mt-0.5">#{transaction.txnId} • {transaction.party.name}</p>
              </div>
              <button onClick={onClose} className="flex h-8 w-8 items-center justify-center rounded-lg hover:bg-zinc-100 dark:hover:bg-zinc-800 transition-colors">
                <X className="h-4 w-4 text-zinc-400" />
              </button>
            </div>

            {/* Transaction preview */}
            <div className="px-5 py-3 bg-zinc-50/50 dark:bg-zinc-800/30 flex items-center gap-3 text-xs border-b border-zinc-100 dark:border-zinc-800">
              <Badge variant={transaction.txnType === "Sale" ? "success" : transaction.txnType === "Expense" ? "danger" : "info"}>{transaction.txnType}</Badge>
              <span className="text-zinc-500">{new Date(transaction.txnDate).toLocaleDateString("en-IN")}</span>
              <span className="text-zinc-500">Bill: {transaction.billNo || "—"}</span>
              <span className="ml-auto font-mono font-semibold text-zinc-900 dark:text-zinc-100">₹{Number(transaction.amount).toLocaleString("en-IN")}</span>
            </div>

            {/* Field selector tabs */}
            <div className="flex gap-1 px-5 py-3 border-b border-zinc-100 dark:border-zinc-800">
              {fields.map((f) => (
                <button
                  key={f.key}
                  onClick={() => setSelectedField(f.key)}
                  className={cn(
                    "flex items-center gap-1.5 rounded-lg px-3 py-1.5 text-xs font-medium transition-all",
                    "active:scale-[0.95]",
                    selectedField === f.key
                      ? "bg-zinc-900 text-white dark:bg-white dark:text-zinc-900"
                      : "text-zinc-500 hover:bg-zinc-100 dark:hover:bg-zinc-800"
                  )}
                >
                  <f.icon className="h-3 w-3" />
                  {f.label}
                </button>
              ))}
            </div>

            {/* Input area */}
            <div className="px-5 py-5">
              <label className="mb-2 block text-[11px] font-medium uppercase tracking-wider text-zinc-500">
                New {fields.find(f => f.key === selectedField)?.label}
              </label>
              {selectedField === "payment_mode" ? (
                <Select
                  value={value}
                  onChange={(e) => setValue(e.target.value)}
                  onKeyDown={handleKeyDown}
                  className="text-base h-12"
                >
                  <option value="Cash">Cash</option>
                  <option value="Credit">Credit</option>
                  <option value="UPI">UPI</option>
                  <option value="Bank">Bank</option>
                </Select>
              ) : selectedField === "txn_type" ? (
                <Select
                  value={value}
                  onChange={(e) => setValue(e.target.value)}
                  onKeyDown={handleKeyDown}
                  className="text-base h-12"
                >
                  <option value="Sale">Sale</option>
                  <option value="Sale Return">Sale Return</option>
                  <option value="Expense">Expense</option>
                  <option value="Receipt">Receipt</option>
                </Select>
              ) : (
                <Input
                  ref={inputRef}
                  type={selectedField === "amount" ? "number" : selectedField === "txn_date" ? "date" : "text"}
                  value={value}
                  onChange={(e) => setValue(e.target.value)}
                  onKeyDown={handleKeyDown}
                  className="text-base h-12 font-mono"
                  placeholder={`Enter new ${fields.find(f => f.key === selectedField)?.label?.toLowerCase()}`}
                />
              )}
              <p className="mt-2 text-[11px] text-zinc-400">
                Press <kbd className="rounded bg-zinc-100 px-1.5 py-0.5 text-[10px] font-medium dark:bg-zinc-800">Enter</kbd> to save • <kbd className="rounded bg-zinc-100 px-1.5 py-0.5 text-[10px] font-medium dark:bg-zinc-800">Esc</kbd> to cancel
              </p>
            </div>

            {/* Footer */}
            <div className="flex items-center justify-end gap-2 border-t border-zinc-100 px-5 py-3 dark:border-zinc-800">
              <Button variant="outline" size="sm" onClick={onClose}>Cancel</Button>
              <Button size="sm" onClick={handleSave} disabled={saving}>
                <Save className="h-3.5 w-3.5 mr-1.5" />
                {saving ? "Saving..." : "Save Changes"}
              </Button>
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}

function cn(...classes: (string | boolean | undefined)[]) {
  return classes.filter(Boolean).join(" ");
}
