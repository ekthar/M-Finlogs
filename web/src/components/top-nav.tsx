"use client";

import { cn } from "@/lib/utils";
import { useApp } from "@/lib/app-context";
import Link from "next/link";
import { usePathname } from "next/navigation";
import { useState, useEffect } from "react";
import { motion, AnimatePresence } from "framer-motion";
import {
  LayoutDashboard, PenLine, BookOpen, CalendarDays, FileBarChart,
  Receipt, ArrowLeftRight, TrendingUp, Package, Users, ScrollText,
  Settings, ChevronDown, Search, LogOut, Menu, X, ShoppingCart,
  Wallet, AlertTriangle, DollarSign,
} from "lucide-react";

interface NavItem { label: string; href: string; icon: typeof LayoutDashboard; }

const mainNav: NavItem[] = [
  { label: "Dashboard", href: "/", icon: LayoutDashboard },
  { label: "Daily Entry", href: "/daily-entry", icon: PenLine },
];

const reportItems: NavItem[] = [
  { label: "Party Ledger", href: "/reports/ledger", icon: BookOpen },
  { label: "Day Book", href: "/reports/day-book", icon: CalendarDays },
  { label: "Daily Summary", href: "/reports/daily-summary", icon: FileBarChart },
  { label: "Short Report", href: "/reports/short-report", icon: AlertTriangle },
  { label: "Purchase Report", href: "/reports/purchase", icon: ShoppingCart },
  { label: "Expenses", href: "/reports/expenses", icon: Wallet },
  { label: "Outstanding", href: "/reports/outstanding", icon: Receipt },
  { label: "Trial Balance", href: "/reports/trial-balance", icon: ArrowLeftRight },
  { label: "P & L", href: "/reports/profit-loss", icon: TrendingUp },
];

const inventoryItems: NavItem[] = [
  { label: "Inventory", href: "/inventory", icon: Package },
  { label: "Stock Value", href: "/inventory/stock-value", icon: DollarSign },
];

const mgmtItems: NavItem[] = [
  { label: "Parties", href: "/parties", icon: Users },
  { label: "Audit Logs", href: "/audit", icon: ScrollText },
  { label: "Settings", href: "/settings", icon: Settings },
];

function NavLink({ item }: { item: NavItem }) {
  const pathname = usePathname();
  const isActive = pathname === item.href || (item.href !== "/" && pathname.startsWith(item.href));
  return (
    <Link
      href={item.href}
      className={cn(
        "flex items-center gap-2 rounded-lg px-3 py-2 text-[13px] font-medium transition-all duration-150",
        "active:scale-[0.97]",
        isActive
          ? "bg-zinc-900 text-white dark:bg-white dark:text-zinc-900"
          : "text-zinc-600 hover:bg-zinc-100 hover:text-zinc-900 dark:text-zinc-400 dark:hover:bg-zinc-800 dark:hover:text-white"
      )}
    >
      <item.icon className="h-4 w-4" />
      {item.label}
    </Link>
  );
}

function DropdownMenu({ label, items, icon: Icon }: { label: string; items: NavItem[]; icon: typeof LayoutDashboard }) {
  const [open, setOpen] = useState(false);
  const pathname = usePathname();
  const hasActive = items.some((i) => pathname === i.href || (i.href !== "/" && pathname.startsWith(i.href)));

  return (
    <div className="relative" onMouseEnter={() => setOpen(true)} onMouseLeave={() => setOpen(false)}>
      <button
        className={cn(
          "flex items-center gap-1.5 rounded-lg px-3 py-2 text-[13px] font-medium transition-all duration-150",
          "active:scale-[0.97]",
          hasActive
            ? "bg-zinc-900 text-white dark:bg-white dark:text-zinc-900"
            : "text-zinc-600 hover:bg-zinc-100 hover:text-zinc-900 dark:text-zinc-400 dark:hover:bg-zinc-800 dark:hover:text-white"
        )}
        onClick={() => setOpen(!open)}
      >
        <Icon className="h-4 w-4" />
        {label}
        <ChevronDown className={cn("h-3 w-3 transition-transform", open && "rotate-180")} />
      </button>
      <AnimatePresence>
        {open && (
          <motion.div
            initial={{ opacity: 0, y: 4, scale: 0.96 }}
            animate={{ opacity: 1, y: 0, scale: 1 }}
            exit={{ opacity: 0, y: 4, scale: 0.96 }}
            transition={{ type: "spring" as const, bounce: 0, duration: 0.2 }}
            className="absolute left-0 top-full z-50 mt-1 min-w-[200px] rounded-xl border border-zinc-200/60 bg-white/90 p-1.5 shadow-xl backdrop-blur-2xl dark:border-zinc-700/60 dark:bg-zinc-900/90"
          >
            {items.map((item) => (
              <Link
                key={item.href}
                href={item.href}
                onClick={() => setOpen(false)}
                className={cn(
                  "flex items-center gap-2.5 rounded-lg px-3 py-2 text-[13px] font-medium transition-colors",
                  pathname === item.href || (item.href !== "/" && pathname.startsWith(item.href))
                    ? "bg-zinc-100 text-zinc-900 dark:bg-zinc-800 dark:text-white"
                    : "text-zinc-600 hover:bg-zinc-50 hover:text-zinc-900 dark:text-zinc-400 dark:hover:bg-zinc-800 dark:hover:text-white"
                )}
              >
                <item.icon className="h-4 w-4 text-zinc-400" />
                {item.label}
              </Link>
            ))}
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
}

export function TopNav() {
  const { financialYear, setFinancialYear, companyId } = useApp();
  const [mobileOpen, setMobileOpen] = useState(false);
  const [fyOptions, setFyOptions] = useState<string[]>([financialYear]);

  // Load FY options from API
  useEffect(() => {
    fetch(`/api/financial-years?companyId=${companyId}`)
      .then(r => r.json())
      .then(d => { if (d.years?.length) setFyOptions(d.years); })
      .catch(() => {});
  }, [companyId]);

  return (
    <>
      <nav className="sticky top-0 z-40 border-b border-zinc-200/40 bg-white/70 backdrop-blur-2xl dark:border-zinc-700/40 dark:bg-zinc-900/70">
        {/* Primary bar */}
        <div className="flex h-14 items-center gap-1 px-4">
          {/* Brand */}
          <Link href="/" className="flex items-center gap-2.5 mr-4">
            <div className="flex h-8 w-8 items-center justify-center rounded-lg bg-gradient-to-br from-zinc-900 to-zinc-700 dark:from-white dark:to-zinc-300">
              <span className="text-xs font-bold text-white dark:text-zinc-900">M</span>
            </div>
            <span className="hidden sm:inline text-sm font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">M-Finlogs</span>
          </Link>

          {/* Desktop navigation */}
          <div className="hidden lg:flex items-center gap-0.5">
            {mainNav.map((item) => <NavLink key={item.href} item={item} />)}
            <DropdownMenu label="Reports" items={reportItems} icon={FileBarChart} />
            <DropdownMenu label="Inventory" items={inventoryItems} icon={Package} />
            <DropdownMenu label="Manage" items={mgmtItems} icon={Settings} />
          </div>

          {/* Right side */}
          <div className="ml-auto flex items-center gap-2">
            {/* FY selector */}
            <select
              value={financialYear}
              onChange={(e) => setFinancialYear(e.target.value)}
              className="hidden sm:block h-8 rounded-lg border border-zinc-200 bg-white/80 px-2 text-xs font-medium text-zinc-700 dark:border-zinc-700 dark:bg-zinc-800 dark:text-zinc-300"
            >
              {fyOptions.map(fy => <option key={fy} value={fy}>FY {fy.replace("-", "-")}</option>)}
            </select>

            {/* Search trigger */}
            <button
              onClick={() => { const e = new KeyboardEvent("keydown", { key: "k", metaKey: true, bubbles: true }); window.dispatchEvent(e); }}
              className="flex h-8 items-center gap-1.5 rounded-lg border border-zinc-200 px-2.5 text-xs text-zinc-400 hover:text-zinc-600 hover:border-zinc-300 transition-colors dark:border-zinc-700"
            >
              <Search className="h-3.5 w-3.5" />
              <span className="hidden sm:inline">Search</span>
              <kbd className="hidden sm:inline text-[10px] bg-zinc-100 dark:bg-zinc-800 px-1 py-0.5 rounded">⌘K</kbd>
            </button>

            {/* User */}
            <div className="flex h-8 w-8 items-center justify-center rounded-full bg-zinc-100 dark:bg-zinc-800">
              <span className="text-[11px] font-semibold text-zinc-600 dark:text-zinc-300">A</span>
            </div>

            {/* Mobile toggle */}
            <button
              onClick={() => setMobileOpen(!mobileOpen)}
              className="lg:hidden flex h-8 w-8 items-center justify-center rounded-lg hover:bg-zinc-100 dark:hover:bg-zinc-800"
            >
              {mobileOpen ? <X className="h-5 w-5" /> : <Menu className="h-5 w-5" />}
            </button>
          </div>
        </div>
      </nav>

      {/* Mobile menu */}
      <AnimatePresence>
        {mobileOpen && (
          <motion.div
            initial={{ opacity: 0, height: 0 }}
            animate={{ opacity: 1, height: "auto" }}
            exit={{ opacity: 0, height: 0 }}
            transition={{ type: "spring" as const, bounce: 0, duration: 0.3 }}
            className="lg:hidden border-b border-zinc-200/60 bg-white/90 backdrop-blur-xl overflow-hidden dark:border-zinc-700/60 dark:bg-zinc-900/90"
          >
            <div className="grid grid-cols-2 gap-1 p-3">
              {[...mainNav, ...reportItems, ...inventoryItems, ...mgmtItems].map((item) => (
                <Link
                  key={item.href}
                  href={item.href}
                  onClick={() => setMobileOpen(false)}
                  className="flex items-center gap-2 rounded-lg px-3 py-2.5 text-[13px] font-medium text-zinc-600 hover:bg-zinc-100 dark:text-zinc-400 dark:hover:bg-zinc-800"
                >
                  <item.icon className="h-4 w-4" />
                  {item.label}
                </Link>
              ))}
            </div>
          </motion.div>
        )}
      </AnimatePresence>
    </>
  );
}
