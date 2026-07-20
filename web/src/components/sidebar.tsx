"use client";

import { cn } from "@/lib/utils";
import { motion, AnimatePresence } from "framer-motion";
import Link from "next/link";
import { usePathname } from "next/navigation";
import { useState } from "react";
import {
  LayoutDashboard,
  PenLine,
  BookOpen,
  CalendarDays,
  FileBarChart,
  AlertTriangle,
  ArrowLeftRight,
  TrendingUp,
  Package,
  DollarSign,
  Users,
  ScrollText,
  Settings,
  ChevronLeft,
  Receipt,
  ShoppingCart,
  Wallet,
  LogOut,
} from "lucide-react";

interface NavItem {
  label: string;
  href: string;
  icon: typeof LayoutDashboard;
}

interface NavGroup {
  title: string;
  items: NavItem[];
}

const navigation: NavGroup[] = [
  {
    title: "",
    items: [
      { label: "Dashboard", href: "/", icon: LayoutDashboard },
      { label: "Daily Entry", href: "/daily-entry", icon: PenLine },
    ],
  },
  {
    title: "Reports",
    items: [
      { label: "Party Ledger", href: "/reports/ledger", icon: BookOpen },
      { label: "Day Book", href: "/reports/day-book", icon: CalendarDays },
      { label: "Daily Summary", href: "/reports/daily-summary", icon: FileBarChart },
      { label: "Short Report", href: "/reports/short-report", icon: AlertTriangle },
      { label: "Purchase Report", href: "/reports/purchase", icon: ShoppingCart },
      { label: "Expenses", href: "/reports/expenses", icon: Wallet },
      { label: "Outstanding", href: "/reports/outstanding", icon: Receipt },
      { label: "Trial Balance", href: "/reports/trial-balance", icon: ArrowLeftRight },
      { label: "P & L", href: "/reports/profit-loss", icon: TrendingUp },
    ],
  },
  {
    title: "Inventory",
    items: [
      { label: "Inventory Management", href: "/inventory", icon: Package },
      { label: "Stock Value Report", href: "/inventory/stock-value", icon: DollarSign },
    ],
  },
  {
    title: "Management",
    items: [
      { label: "Parties", href: "/parties", icon: Users },
      { label: "Audit Logs", href: "/audit", icon: ScrollText },
      { label: "Settings", href: "/settings", icon: Settings },
    ],
  },
];

export function Sidebar() {
  const pathname = usePathname();
  const [collapsed, setCollapsed] = useState(false);

  return (
    <motion.aside
      initial={false}
      animate={{ width: collapsed ? 72 : 260 }}
      transition={{ type: "spring", bounce: 0, duration: 0.35 }}
      className={cn(
        "fixed left-0 top-0 z-40 flex h-screen flex-col",
        "border-r border-zinc-200/40",
        "bg-white/60 backdrop-blur-2xl",
        "dark:border-zinc-700/40 dark:bg-zinc-900/60"
      )}
    >
      {/* Brand */}
      <div className="flex h-16 items-center gap-3 px-5 border-b border-zinc-200/40 dark:border-zinc-700/40">
        <div className="flex h-9 w-9 shrink-0 items-center justify-center rounded-xl bg-gradient-to-br from-zinc-900 to-zinc-700 dark:from-white dark:to-zinc-300">
          <span className="text-sm font-bold text-white dark:text-zinc-900">M</span>
        </div>
        <AnimatePresence>
          {!collapsed && (
            <motion.span
              initial={{ opacity: 0, x: -8 }}
              animate={{ opacity: 1, x: 0 }}
              exit={{ opacity: 0, x: -8 }}
              transition={{ type: "spring", bounce: 0, duration: 0.25 }}
              className="text-base font-semibold tracking-tight text-zinc-900 dark:text-zinc-100"
            >
              M-Finlogs
            </motion.span>
          )}
        </AnimatePresence>

        <button
          onClick={() => setCollapsed(!collapsed)}
          className={cn(
            "ml-auto flex h-7 w-7 items-center justify-center rounded-lg",
            "text-zinc-400 hover:text-zinc-600 hover:bg-zinc-100",
            "transition-all duration-200",
            "dark:hover:text-zinc-300 dark:hover:bg-zinc-800",
            collapsed && "ml-0"
          )}
        >
          <ChevronLeft
            className={cn(
              "h-4 w-4 transition-transform duration-300",
              collapsed && "rotate-180"
            )}
          />
        </button>
      </div>

      {/* Navigation */}
      <nav className="flex-1 overflow-y-auto px-3 py-4 space-y-6 scrollbar-none">
        {navigation.map((group, gi) => (
          <div key={gi}>
            <AnimatePresence>
              {!collapsed && group.title && (
                <motion.div
                  initial={{ opacity: 0 }}
                  animate={{ opacity: 1 }}
                  exit={{ opacity: 0 }}
                  className="mb-2 px-3 text-[10px] font-semibold uppercase tracking-widest text-zinc-400 dark:text-zinc-500"
                >
                  {group.title}
                </motion.div>
              )}
            </AnimatePresence>

            <div className="space-y-0.5">
              {group.items.map((item) => {
                const isActive =
                  pathname === item.href ||
                  (item.href !== "/" && pathname.startsWith(item.href));

                return (
                  <Link
                    key={item.href}
                    href={item.href}
                    className={cn(
                      "group relative flex items-center gap-3 rounded-xl px-3 py-2.5",
                      "text-sm font-medium transition-all duration-200 ease-out",
                      "active:scale-[0.97] active:transition-transform active:duration-100",
                      isActive
                        ? "bg-zinc-900 text-white shadow-sm dark:bg-white dark:text-zinc-900"
                        : "text-zinc-600 hover:bg-zinc-100 hover:text-zinc-900 dark:text-zinc-400 dark:hover:bg-zinc-800 dark:hover:text-zinc-100",
                      collapsed && "justify-center px-0"
                    )}
                  >
                    <item.icon className={cn("h-[18px] w-[18px] shrink-0", collapsed && "h-5 w-5")} />
                    <AnimatePresence>
                      {!collapsed && (
                        <motion.span
                          initial={{ opacity: 0, x: -4 }}
                          animate={{ opacity: 1, x: 0 }}
                          exit={{ opacity: 0, x: -4 }}
                          transition={{ type: "spring", bounce: 0, duration: 0.2 }}
                          className="truncate"
                        >
                          {item.label}
                        </motion.span>
                      )}
                    </AnimatePresence>

                    {/* Tooltip for collapsed state */}
                    {collapsed && (
                      <div className="absolute left-full ml-2 hidden rounded-lg bg-zinc-900 px-2.5 py-1.5 text-xs text-white shadow-lg group-hover:block dark:bg-white dark:text-zinc-900">
                        {item.label}
                      </div>
                    )}
                  </Link>
                );
              })}
            </div>
          </div>
        ))}
      </nav>

      {/* Footer */}
      <div className="border-t border-zinc-200/40 p-3 dark:border-zinc-700/40">
        <button
          className={cn(
            "flex w-full items-center gap-3 rounded-xl px-3 py-2.5",
            "text-sm font-medium text-red-500 hover:bg-red-50 dark:hover:bg-red-900/20",
            "transition-all duration-200 active:scale-[0.97]",
            collapsed && "justify-center px-0"
          )}
        >
          <LogOut className="h-[18px] w-[18px] shrink-0" />
          <AnimatePresence>
            {!collapsed && (
              <motion.span
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                exit={{ opacity: 0 }}
              >
                Logout
              </motion.span>
            )}
          </AnimatePresence>
        </button>
      </div>
    </motion.aside>
  );
}
