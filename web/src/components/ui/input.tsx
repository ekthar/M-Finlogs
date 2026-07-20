"use client";

import { cn } from "@/lib/utils";
import { forwardRef, type InputHTMLAttributes } from "react";

export interface InputProps extends InputHTMLAttributes<HTMLInputElement> {}

/**
 * Apple-style input:
 * - Subtle backdrop-blur for glass feel
 * - Ring appears on focus (not border change — more visible)
 * - Transition is fast on focus (100ms), gentle on blur (200ms)
 */
const Input = forwardRef<HTMLInputElement, InputProps>(
  ({ className, type, ...props }, ref) => (
    <input
      type={type}
      className={cn(
        "flex h-9 w-full rounded-[10px] px-3 py-2 text-[13px]",
        "border border-zinc-200/80 bg-white/80",
        "shadow-[0_1px_2px_rgba(0,0,0,0.03)]",
        "backdrop-blur-sm",
        "placeholder:text-zinc-400",
        "transition-[border-color,box-shadow] duration-200 ease-out",
        "focus:outline-none focus:border-blue-400/60 focus:ring-[3px] focus:ring-blue-500/10",
        "focus:duration-100",
        "disabled:cursor-not-allowed disabled:opacity-40",
        "dark:border-zinc-700/80 dark:bg-zinc-800/80 dark:text-zinc-100",
        "dark:focus:border-blue-400/50 dark:focus:ring-blue-400/10",
        className
      )}
      ref={ref}
      {...props}
    />
  )
);
Input.displayName = "Input";

export { Input };
