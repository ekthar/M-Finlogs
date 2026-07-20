"use client";

import { cn } from "@/lib/utils";
import { motion } from "framer-motion";
import type { LucideIcon } from "lucide-react";

interface MetricCardProps {
  title: string;
  value: string;
  icon: LucideIcon;
  trend?: { value: number; label: string };
  className?: string;
}

export function MetricCard({ title, value, icon: Icon, trend, className }: MetricCardProps) {
  return (
    <motion.div
      initial={{ opacity: 0, y: 12 }}
      animate={{ opacity: 1, y: 0 }}
      transition={{ type: "spring", bounce: 0, duration: 0.4 }}
      className={cn(
        "group relative overflow-hidden rounded-2xl border border-zinc-200/60 bg-white/70 p-5",
        "backdrop-blur-xl shadow-sm",
        "transition-all duration-300 ease-out",
        "hover:shadow-md hover:border-zinc-300/80 hover:bg-white/90",
        "dark:border-zinc-700/60 dark:bg-zinc-900/70 dark:hover:bg-zinc-800/80",
        className
      )}
    >
      {/* Subtle gradient overlay on hover */}
      <div className="absolute inset-0 bg-gradient-to-br from-zinc-50/50 to-transparent opacity-0 group-hover:opacity-100 transition-opacity duration-300 dark:from-zinc-800/30" />

      <div className="relative z-10">
        <div className="flex items-center justify-between mb-3">
          <span className="text-xs font-medium uppercase tracking-wider text-zinc-500 dark:text-zinc-400">
            {title}
          </span>
          <div className="flex h-8 w-8 items-center justify-center rounded-lg bg-zinc-100 dark:bg-zinc-800">
            <Icon className="h-4 w-4 text-zinc-600 dark:text-zinc-300" />
          </div>
        </div>

        <div className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">
          {value}
        </div>

        {trend && (
          <div className="mt-2 flex items-center gap-1">
            <span
              className={cn(
                "text-xs font-medium",
                trend.value >= 0 ? "text-emerald-600" : "text-red-500"
              )}
            >
              {trend.value >= 0 ? "+" : ""}
              {trend.value}%
            </span>
            <span className="text-xs text-zinc-400">{trend.label}</span>
          </div>
        )}
      </div>
    </motion.div>
  );
}
