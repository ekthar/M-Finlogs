"use client";

import { cn } from "@/lib/utils";
import { forwardRef, type SelectHTMLAttributes } from "react";

export interface SelectProps extends SelectHTMLAttributes<HTMLSelectElement> {}

const Select = forwardRef<HTMLSelectElement, SelectProps>(
  ({ className, children, ...props }, ref) => (
    <select
      className={cn(
        "flex h-9 w-full rounded-[10px] px-3 py-2 text-[13px]",
        "border border-zinc-200/80 bg-white/80",
        "shadow-[0_1px_2px_rgba(0,0,0,0.03)]",
        "backdrop-blur-sm appearance-none",
        "bg-[url('data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMTIiIGhlaWdodD0iOCIgdmlld0JveD0iMCAwIDEyIDgiIGZpbGw9Im5vbmUiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyI+PHBhdGggZD0iTTEgMS41TDYgNi41TDExIDEuNSIgc3Ryb2tlPSIjNzE3MTcxIiBzdHJva2Utd2lkdGg9IjEuNSIgc3Ryb2tlLWxpbmVjYXA9InJvdW5kIiBzdHJva2UtbGluZWpvaW49InJvdW5kIi8+PC9zdmc+')] bg-[length:12px] bg-[right_12px_center] bg-no-repeat",
        "transition-[border-color,box-shadow] duration-200 ease-out",
        "focus:outline-none focus:border-blue-400/60 focus:ring-[3px] focus:ring-blue-500/10",
        "disabled:cursor-not-allowed disabled:opacity-40",
        "dark:border-zinc-700/80 dark:bg-zinc-800/80 dark:text-zinc-100",
        "dark:focus:border-blue-400/50 dark:focus:ring-blue-400/10",
        className
      )}
      ref={ref}
      {...props}
    >
      {children}
    </select>
  )
);
Select.displayName = "Select";

export { Select };
