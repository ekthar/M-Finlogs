"use client";

import { cn } from "@/lib/utils";
import { forwardRef, type ButtonHTMLAttributes } from "react";
import { cva, type VariantProps } from "class-variance-authority";

/**
 * Apple-style button:
 * - Responds on pointer-down (scale + shadow reduce)
 * - Spring-based release (handled by active: pseudo)
 * - No fixed transition duration on press (instant)
 * - Gentle transition on release (150ms spring-like ease-out)
 */
const buttonVariants = cva(
  [
    "inline-flex items-center justify-center gap-2 rounded-xl font-medium select-none",
    "disabled:pointer-events-none disabled:opacity-40",
    // Press physics: instant scale on press, gentle release
    "transition-[transform,box-shadow] duration-150 ease-out",
    "active:scale-[0.96] active:shadow-none active:duration-[50ms]",
  ].join(" "),
  {
    variants: {
      variant: {
        default:
          "bg-zinc-900 text-white shadow-[0_1px_2px_rgba(0,0,0,0.2),0_2px_8px_rgba(0,0,0,0.1)] hover:bg-zinc-800 dark:bg-zinc-100 dark:text-zinc-900 dark:hover:bg-white",
        destructive:
          "bg-red-500 text-white shadow-[0_1px_2px_rgba(239,68,68,0.3)] hover:bg-red-600 dark:bg-red-600 dark:hover:bg-red-500",
        outline:
          "border border-zinc-200/80 bg-white/60 backdrop-blur-sm shadow-[0_1px_2px_rgba(0,0,0,0.03)] hover:bg-zinc-50 hover:border-zinc-300 dark:border-zinc-700 dark:bg-zinc-800/60 dark:hover:bg-zinc-700/60 dark:hover:border-zinc-600",
        secondary:
          "bg-zinc-100 text-zinc-800 hover:bg-zinc-200 dark:bg-zinc-800 dark:text-zinc-200 dark:hover:bg-zinc-700",
        ghost:
          "hover:bg-zinc-100/80 dark:hover:bg-zinc-800/80",
        link: "text-zinc-900 underline-offset-4 hover:underline dark:text-zinc-100",
      },
      size: {
        default: "h-9 px-4 py-2 text-[13px]",
        sm: "h-8 px-3 text-[12px]",
        lg: "h-11 px-6 text-[14px]",
        icon: "h-9 w-9",
      },
    },
    defaultVariants: {
      variant: "default",
      size: "default",
    },
  }
);

export interface ButtonProps
  extends ButtonHTMLAttributes<HTMLButtonElement>,
    VariantProps<typeof buttonVariants> {}

const Button = forwardRef<HTMLButtonElement, ButtonProps>(
  ({ className, variant, size, ...props }, ref) => (
    <button
      className={cn(buttonVariants({ variant, size, className }))}
      ref={ref}
      {...props}
    />
  )
);
Button.displayName = "Button";

export { Button, buttonVariants };
