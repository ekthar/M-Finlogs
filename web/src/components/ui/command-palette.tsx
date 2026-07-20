"use client";

import { useState, useEffect, useRef } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { useRouter } from "next/navigation";
import { useApp } from "@/lib/app-context";
import {
  LayoutDashboard, PenLine, BookOpen, CalendarDays, FileBarChart,
  Receipt, ArrowLeftRight, TrendingUp, Package, Users, Settings, Search,
  User, Hash,
} from "lucide-react";

interface SearchResult { label: string; href: string; icon: typeof LayoutDashboard; type: "page" | "party" | "transaction"; }

const pages: SearchResult[] = [
  { label: "Dashboard", href: "/", icon: LayoutDashboard, type: "page" },
  { label: "Daily Entry", href: "/daily-entry", icon: PenLine, type: "page" },
  { label: "Party Ledger", href: "/reports/ledger", icon: BookOpen, type: "page" },
  { label: "Day Book", href: "/reports/day-book", icon: CalendarDays, type: "page" },
  { label: "Daily Summary", href: "/reports/daily-summary", icon: FileBarChart, type: "page" },
  { label: "Outstanding", href: "/reports/outstanding", icon: Receipt, type: "page" },
  { label: "Trial Balance", href: "/reports/trial-balance", icon: ArrowLeftRight, type: "page" },
  { label: "Profit & Loss", href: "/reports/profit-loss", icon: TrendingUp, type: "page" },
  { label: "Inventory", href: "/inventory", icon: Package, type: "page" },
  { label: "Parties", href: "/parties", icon: Users, type: "page" },
  { label: "Settings", href: "/settings", icon: Settings, type: "page" },
];

export function CommandPalette() {
  const [open, setOpen] = useState(false);
  const [query, setQuery] = useState("");
  const [results, setResults] = useState<SearchResult[]>(pages);
  const [partyResults, setPartyResults] = useState<SearchResult[]>([]);
  const inputRef = useRef<HTMLInputElement>(null);
  const router = useRouter();
  const { companyId } = useApp();

  useEffect(() => {
    const handleKey = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.key === "k") { e.preventDefault(); setOpen((o) => !o); }
      if (e.key === "Escape") setOpen(false);
    };
    window.addEventListener("keydown", handleKey);
    return () => window.removeEventListener("keydown", handleKey);
  }, []);

  useEffect(() => {
    if (open) { inputRef.current?.focus(); setQuery(""); setPartyResults([]); }
  }, [open]);

  // Search parties when query length > 1
  useEffect(() => {
    if (!query || query.length < 2) { setPartyResults([]); return; }
    const timeout = setTimeout(async () => {
      try {
        const res = await fetch(`/api/parties?companyId=${companyId}`);
        const data = await res.json();
        const matched = (data.parties || [])
          .filter((p: { name: string }) => p.name.toLowerCase().includes(query.toLowerCase()))
          .slice(0, 5)
          .map((p: { name: string }) => ({
            label: p.name,
            href: `/reports/ledger?party=${encodeURIComponent(p.name)}`,
            icon: User,
            type: "party" as const,
          }));
        setPartyResults(matched);
      } catch {}
    }, 200);
    return () => clearTimeout(timeout);
  }, [query, companyId]);

  const filteredPages = query
    ? pages.filter((p) => p.label.toLowerCase().includes(query.toLowerCase()))
    : pages;

  const allResults = [...partyResults, ...filteredPages];

  const navigate = (href: string) => { router.push(href); setOpen(false); };

  return (
    <AnimatePresence>
      {open && (
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          className="fixed inset-0 z-[60] flex items-start justify-center pt-[18vh] bg-black/30 backdrop-blur-sm"
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
                placeholder="Search pages, parties, actions..."
                className="flex-1 h-12 bg-transparent text-sm outline-none placeholder:text-zinc-400 dark:text-zinc-100"
                onKeyDown={(e) => { if (e.key === "Enter" && allResults.length > 0) navigate(allResults[0].href); }}
              />
              <kbd className="text-[10px] font-medium text-zinc-400 bg-zinc-100 dark:bg-zinc-800 px-1.5 py-0.5 rounded">ESC</kbd>
            </div>
            <div className="max-h-80 overflow-y-auto p-2">
              {partyResults.length > 0 && (
                <div className="mb-2">
                  <p className="px-3 py-1 text-[10px] font-semibold uppercase tracking-wider text-zinc-400">Parties</p>
                  {partyResults.map((p) => (
                    <button key={p.href} onClick={() => navigate(p.href)} className="flex w-full items-center gap-3 rounded-xl px-3 py-2.5 text-sm text-zinc-700 hover:bg-zinc-100 dark:text-zinc-300 dark:hover:bg-zinc-800 transition-colors">
                      <User className="h-4 w-4 text-blue-500" /> {p.label}
                      <span className="ml-auto text-[10px] text-zinc-400">→ Ledger</span>
                    </button>
                  ))}
                </div>
              )}
              <div>
                {partyResults.length > 0 && <p className="px-3 py-1 text-[10px] font-semibold uppercase tracking-wider text-zinc-400">Pages</p>}
                {filteredPages.map((p) => (
                  <button key={p.href} onClick={() => navigate(p.href)} className="flex w-full items-center gap-3 rounded-xl px-3 py-2.5 text-sm text-zinc-700 hover:bg-zinc-100 dark:text-zinc-300 dark:hover:bg-zinc-800 transition-colors">
                    <p.icon className="h-4 w-4 text-zinc-400" /> {p.label}
                  </button>
                ))}
              </div>
              {allResults.length === 0 && (
                <p className="text-center text-sm text-zinc-400 py-6">No results for &ldquo;{query}&rdquo;</p>
              )}
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}
