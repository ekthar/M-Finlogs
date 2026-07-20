"use client";

import { cn } from "@/lib/utils";
import { Select } from "@/components/ui/select";

interface HeaderProps {
  className?: string;
}

export function Header({ className }: HeaderProps) {
  return (
    <header
      className={cn(
        "sticky top-0 z-30 flex h-16 items-center justify-between gap-4 px-6",
        "border-b border-zinc-200/40",
        "bg-white/60 backdrop-blur-2xl",
        "dark:border-zinc-700/40 dark:bg-zinc-900/60",
        className
      )}
    >
      {/* Left: Context */}
      <div className="flex items-center gap-4">
        <div className="flex items-center gap-2">
          <label className="text-xs font-medium text-zinc-500 dark:text-zinc-400">FY:</label>
          <Select className="h-8 w-[140px] text-xs">
            <option>2026-2027</option>
            <option>2025-2026</option>
            <option>2024-2025</option>
          </Select>
        </div>
        <div className="h-5 w-px bg-zinc-200 dark:bg-zinc-700" />
        <div className="flex items-center gap-2">
          <label className="text-xs font-medium text-zinc-500 dark:text-zinc-400">Company:</label>
          <Select className="h-8 w-[180px] text-xs">
            <option>Default Company</option>
          </Select>
        </div>
      </div>

      {/* Right: User info */}
      <div className="flex items-center gap-3">
        <div className="flex h-8 w-8 items-center justify-center rounded-full bg-zinc-100 dark:bg-zinc-800">
          <span className="text-xs font-semibold text-zinc-600 dark:text-zinc-300">A</span>
        </div>
        <div className="hidden sm:block">
          <p className="text-sm font-medium text-zinc-900 dark:text-zinc-100">admin</p>
          <p className="text-[10px] text-zinc-400">Administrator</p>
        </div>
      </div>
    </header>
  );
}
