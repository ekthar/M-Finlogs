"use client";

import { cn } from "@/lib/utils";
import { forwardRef, type InputHTMLAttributes } from "react";

export interface InputProps extends InputHTMLAttributes<HTMLInputElement> {}

const Input = forwardRef<HTMLInputElement, InputProps>(
  ({ className, type, ...props }, ref) => {
    return (
      <input
        type={type}
        className={cn(
          "flex h-10 w-full rounded-xl border border-zinc-200 bg-white/80 px-3 py-2 text-sm",
          "backdrop-blur-sm shadow-sm",
          "transition-all duration-200 ease-out",
          "placeholder:text-zinc-400",
          "focus:outline-none focus:ring-2 focus:ring-zinc-900/10 focus:border-zinc-400",
          "disabled:cursor-not-allowed disabled:opacity-50",
          "dark:border-zinc-700 dark:bg-zinc-800/80 dark:text-zinc-100",
          "dark:focus:ring-white/10 dark:focus:border-zinc-500",
          className
        )}
        ref={ref}
        {...props}
      />
    );
  }
);
Input.displayName = "Input";

export { Input };
