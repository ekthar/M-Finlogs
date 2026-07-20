"use client";

import { useState, useEffect, useRef } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { useRouter } from "next/navigation";
import { Input } from "@/components/ui/input";
import {
  LayoutDashboard, PenLine, BookOpen, CalendarDays, FileBarChart,
  Receipt, ArrowLeftRight, TrendingUp, Package, Users, Settings, Search,
} from "lucide-react";

const pages = [
  { label: "Dashboard", href: "/", icon: LayoutDashboard },
  { label: "Daily Entry", href: "/daily-entry", icon: PenLine },
  { label: "Party Ledger", href: "/reports/ledger", icon: BookOpen },
  { label: "Day Book", href: "/reports/day-book", icon: CalendarDays },
  { label: "Daily Summary", href: "/reports/daily-summary", icon: FileBarChart },
  { label: "Outstanding", href: "/reports/outstanding", icon: Receipt },
  { label: "Trial Balance", href: "/reports/trial-balance", icon: ArrowLeftRight },
  { label: "Profit & Loss", href: "/reports/profit-loss", icon: TrendingUp },
  { label: "Inventory", href: "/inventory", icon: Package },
  { label: "Parties", href: "/parties", icon: Users },
  { label: "Settings", href: "/settings", icon: Settings },
];

export function CommandPalette() {
  const [open, setOpen] = useState(false);
  const [query, setQuery] = useState("");
  const inputRef = useRef<HTMLInputElement>(null);
  const router = useRouter();

  useEffect(() => {
    const handleKey = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.key === "k") {
        e.preventDefault();
        setOpen((o) => !o);
      }
      if (e.key === "Escape") setOpen(false);
    };
    window.addEventListener("keydown", handleKey);
    return () => window.removeEventListener("keydown", handleKey);
  }, []);

  useEffect(() => {
    if (open) { inputRef.current?.focus(); setQuery(""); }
  }, [open]);

  const filtered = query
    ? pages.filter((p) => p.label.toLowerCase().includes(query.toLowerCase()))
    : pages;

  const navigate = (href: string) => { router.push(href); setOpen(false); };

  return (
    <AnimatePresence>
      {open && (
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          className="fixed inset-0 z-[60] flex items-start justify-center pt-[20vh] bg-black/30 backdrop-blur-sm"
          onClick={() => setOpen(false)}
        >
          <motion.div
            initial={{ opacity: 0, scale: 0.96, y: -8 }}
            animate={{ opacity: 1, scale: 1, y: 0 }}
            exit={{ opacity: 0, scale: 0.96, y: -8 }}
            transition={{ type: "spring" as const, bounce: 0, duration: 0.25 }}
            className="w-full max-w-lg rounded-2xl border border-zinc-200 bg-white shadow-2xl overflow-hidden dark:border-zinc-700 dark:bg-zinc-900"
            onClick={(e) => e.stopPropagation()}
          >
            <div className="flex items-center gap-3 border-b border-zinc-100 px-4 dark:border-zinc-800">
              <Search className="h-4 w-4 text-zinc-400 shrink-0" />
              <input
                ref={inputRef}
                value={query}
                onChange={(e) => setQuery(e.target.value)}
                placeholder="Search pages, actions..."
                className="flex-1 h-12 bg-transparent text-sm outline-none placeholder:text-zinc-400 dark:text-zinc-100"
              />
              <kbd className="text-[10px] font-medium text-zinc-400 bg-zinc-100 dark:bg-zinc-800 px-1.5 py-0.5 rounded">ESC</kbd>
            </div>
            <div className="max-h-72 overflow-y-auto p-2">
              {filtered.map((p) => (
                <button
                  key={p.href}
                  onClick={() => navigate(p.href)}
                  className="flex w-full items-center gap-3 rounded-xl px-3 py-2.5 text-sm text-zinc-700 hover:bg-zinc-100 dark:text-zinc-300 dark:hover:bg-zinc-800 transition-colors"
                >
                  <p.icon className="h-4 w-4 text-zinc-400" />
                  {p.label}
                </button>
              ))}
              {filtered.length === 0 && (
                <p className="text-center text-sm text-zinc-400 py-6">No results found</p>
              )}
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}
