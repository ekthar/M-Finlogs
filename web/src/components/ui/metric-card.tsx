"use client";

import { cn } from "@/lib/utils";
import { AnimatedNumber } from "@/components/ui/animated-number";
import { Sparkline } from "@/components/ui/sparkline";
import { motion } from "framer-motion";
import { springs } from "@/lib/design-tokens";
import type { LucideIcon } from "lucide-react";

interface MetricCardProps {
  title: string;
  value: number;
  prefix?: string;
  icon: LucideIcon;
  trend?: { value: number; label: string };
  sparkData?: number[];
  sparkColor?: string;
  className?: string;
}

export function MetricCard({ title, value, prefix = "₹", icon: Icon, trend, sparkData, sparkColor, className }: MetricCardProps) {
  return (
    <motion.div
      initial={{ opacity: 0, y: 16, scale: 0.97 }}
      animate={{ opacity: 1, y: 0, scale: 1 }}
      transition={springs.default}
      whileHover={{ y: -2, transition: springs.snappy }}
      whileTap={{ scale: 0.98, transition: springs.micro }}
      className={cn(
        "group relative overflow-hidden rounded-2xl p-5",
        "border border-zinc-200/50 bg-white/72",
        "backdrop-blur-xl shadow-sm",
        "hover:shadow-md hover:border-zinc-200/80",
        "dark:border-zinc-700/50 dark:bg-zinc-900/72",
        "dark:hover:border-zinc-600/60",
        className
      )}
    >
      {/* Gradient shimmer on hover */}
      <div className="absolute inset-0 bg-gradient-to-br from-white/40 via-transparent to-transparent opacity-0 group-hover:opacity-100 transition-opacity duration-500 dark:from-zinc-800/20" />

      <div className="relative z-10">
        <div className="flex items-center justify-between mb-3">
          <span className="text-[11px] font-semibold uppercase tracking-widest text-zinc-500 dark:text-zinc-400">
            {title}
          </span>
          <div className="flex h-8 w-8 items-center justify-center rounded-xl bg-zinc-100/80 dark:bg-zinc-800/80 transition-colors group-hover:bg-zinc-200/80 dark:group-hover:bg-zinc-700/80">
            <Icon className="h-4 w-4 text-zinc-500 dark:text-zinc-400" />
          </div>
        </div>

        <AnimatedNumber
          value={value}
          prefix={prefix}
          className="text-[1.65rem] font-semibold tracking-tight text-zinc-900 dark:text-zinc-100"
        />

        {sparkData && sparkData.length > 1 && (
          <div className="mt-2">
            <Sparkline data={sparkData} color={sparkColor || "#10b981"} width={80} height={20} />
          </div>
        )}

        {trend && (
          <div className="mt-2.5 flex items-center gap-1.5">
            <span className={cn(
              "inline-flex items-center rounded-md px-1.5 py-0.5 text-[10px] font-semibold",
              trend.value >= 0 
                ? "bg-emerald-50 text-emerald-700 dark:bg-emerald-900/30 dark:text-emerald-400" 
                : "bg-red-50 text-red-700 dark:bg-red-900/30 dark:text-red-400"
            )}>
              {trend.value >= 0 ? "↑" : "↓"} {Math.abs(trend.value)}%
            </span>
            <span className="text-[10px] text-zinc-400">{trend.label}</span>
          </div>
        )}
      </div>
    </motion.div>
  );
}
