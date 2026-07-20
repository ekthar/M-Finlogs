"use client";

import { cn } from "@/lib/utils";
import { forwardRef, type ButtonHTMLAttributes } from "react";
import { cva, type VariantProps } from "class-variance-authority";

const buttonVariants = cva(
  "inline-flex items-center justify-center gap-2 rounded-xl font-medium transition-all duration-200 ease-out select-none disabled:pointer-events-none disabled:opacity-50 active:scale-[0.97] active:transition-transform active:duration-100",
  {
    variants: {
      variant: {
        default:
          "bg-zinc-900 text-white shadow-sm hover:bg-zinc-800 dark:bg-white dark:text-zinc-900 dark:hover:bg-zinc-100",
        destructive:
          "bg-red-500/90 text-white shadow-sm hover:bg-red-600 dark:bg-red-600 dark:hover:bg-red-700",
        outline:
          "border border-zinc-200 bg-white/60 backdrop-blur-sm hover:bg-zinc-50 dark:border-zinc-700 dark:bg-zinc-800/60 dark:hover:bg-zinc-700/60",
        secondary:
          "bg-zinc-100 text-zinc-900 hover:bg-zinc-200 dark:bg-zinc-800 dark:text-zinc-100 dark:hover:bg-zinc-700",
        ghost:
          "hover:bg-zinc-100 dark:hover:bg-zinc-800",
        link: "text-zinc-900 underline-offset-4 hover:underline dark:text-zinc-100",
      },
      size: {
        default: "h-10 px-4 py-2 text-sm",
        sm: "h-8 px-3 text-xs",
        lg: "h-12 px-6 text-base",
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
  ({ className, variant, size, ...props }, ref) => {
    return (
      <button
        className={cn(buttonVariants({ variant, size, className }))}
        ref={ref}
        {...props}
      />
    );
  }
);
Button.displayName = "Button";

export { Button, buttonVariants };
