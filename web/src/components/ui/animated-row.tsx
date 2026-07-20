"use client";

import { motion } from "framer-motion";
import { springs } from "@/lib/design-tokens";
import { cn } from "@/lib/utils";

interface AnimatedRowProps {
  children: React.ReactNode;
  isNew?: boolean;
  isDeleting?: boolean;
  className?: string;
}

/**
 * Animated table row wrapper:
 * - New entries: flash green briefly then normalize
 * - Deleting entries: collapse height smoothly
 * - Normal: fade-in on first render
 */
export function AnimatedRow({ children, isNew, isDeleting, className }: AnimatedRowProps) {
  return (
    <motion.tr
      initial={isNew ? { backgroundColor: "rgba(16, 185, 129, 0.15)", opacity: 0, height: 0 } : { opacity: 0 }}
      animate={
        isDeleting
          ? { opacity: 0, height: 0, marginBottom: 0 }
          : { backgroundColor: "rgba(0,0,0,0)", opacity: 1, height: "auto" }
      }
      exit={{ opacity: 0, height: 0 }}
      transition={isDeleting ? { ...springs.snappy } : { ...springs.default }}
      className={cn(
        "border-b border-zinc-100 dark:border-zinc-800",
        "hover:bg-zinc-50/60 dark:hover:bg-zinc-800/40",
        className
      )}
    >
      {children}
    </motion.tr>
  );
}
