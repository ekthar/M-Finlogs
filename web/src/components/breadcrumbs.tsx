"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";
import { ChevronRight, Home } from "lucide-react";

const pathLabels: Record<string, string> = {
  "": "Dashboard",
  "daily-entry": "Daily Entry",
  "reports": "Reports",
  "ledger": "Party Ledger",
  "day-book": "Day Book",
  "daily-summary": "Daily Summary",
  "short-report": "Short Report",
  "purchase": "Purchase Report",
  "expenses": "Expenses",
  "outstanding": "Outstanding",
  "trial-balance": "Trial Balance",
  "profit-loss": "P & L",
  "inventory": "Inventory",
  "stock-value": "Stock Value",
  "parties": "Parties",
  "audit": "Audit Logs",
  "settings": "Settings",
};

export function Breadcrumbs() {
  const pathname = usePathname();
  const segments = pathname.split("/").filter(Boolean);

  if (segments.length === 0) return null; // Don't show on dashboard

  const crumbs = segments.map((seg, i) => {
    const href = "/" + segments.slice(0, i + 1).join("/");
    const label = pathLabels[seg] || seg;
    return { href, label };
  });

  return (
    <nav className="flex items-center gap-1.5 text-[11px] text-zinc-400 mb-4 no-print">
      <Link href="/" className="flex items-center gap-1 hover:text-zinc-600 dark:hover:text-zinc-300 transition-colors">
        <Home className="h-3 w-3" />
      </Link>
      {crumbs.map((crumb, i) => (
        <span key={crumb.href} className="flex items-center gap-1.5">
          <ChevronRight className="h-3 w-3 text-zinc-300 dark:text-zinc-600" />
          {i === crumbs.length - 1 ? (
            <span className="font-medium text-zinc-700 dark:text-zinc-200">{crumb.label}</span>
          ) : (
            <Link href={crumb.href} className="hover:text-zinc-600 dark:hover:text-zinc-300 transition-colors">
              {crumb.label}
            </Link>
          )}
        </span>
      ))}
    </nav>
  );
}
