"use client";

import { motion } from "framer-motion";
import { springs } from "@/lib/design-tokens";
import { FileText, Package, Users, Receipt, BarChart3, Calendar, Search } from "lucide-react";
import { cn } from "@/lib/utils";

type EmptyType = "transactions" | "inventory" | "parties" | "ledger" | "reports" | "audit" | "search";

const config: Record<EmptyType, { icon: typeof FileText; title: string; description: string }> = {
  transactions: { icon: FileText, title: "No transactions yet", description: "Add your first entry using the form above" },
  inventory: { icon: Package, title: "No products tracked", description: "Add products to start tracking daily inventory" },
  parties: { icon: Users, title: "No parties created", description: "Create customers, suppliers, and accounts to get started" },
  ledger: { icon: Receipt, title: "Select a party", description: "Choose a party from the dropdown and click Show Ledger" },
  reports: { icon: BarChart3, title: "No data for this period", description: "Select a date range and click Apply" },
  audit: { icon: Calendar, title: "No audit logs", description: "Activity will appear here as you use the system" },
  search: { icon: Search, title: "No results found", description: "Try a different search term" },
};

interface EmptyStateProps {
  type: EmptyType;
  className?: string;
}

export function EmptyState({ type, className }: EmptyStateProps) {
  const { icon: Icon, title, description } = config[type];

  return (
    <motion.div
      initial={{ opacity: 0, y: 8 }}
      animate={{ opacity: 1, y: 0 }}
      transition={springs.default}
      className={cn("flex flex-col items-center justify-center py-12 px-4", className)}
    >
      <div className="flex h-14 w-14 items-center justify-center rounded-2xl bg-zinc-100 dark:bg-zinc-800 mb-4">
        <Icon className="h-7 w-7 text-zinc-400" />
      </div>
      <h3 className="text-sm font-semibold text-zinc-700 dark:text-zinc-300">{title}</h3>
      <p className="mt-1 text-xs text-zinc-500 text-center max-w-[240px]">{description}</p>
    </motion.div>
  );
}
