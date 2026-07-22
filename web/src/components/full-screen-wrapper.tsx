"use client";

import { useState, useCallback, useEffect, type ReactNode } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { Maximize2, Minimize2 } from "lucide-react";
import { Button } from "@/components/ui/button";
import { cn } from "@/lib/utils";
import { springs } from "@/lib/design-tokens";

interface FullScreenWrapperProps {
  children: ReactNode;
  title?: string;
  className?: string;
}

export function FullScreenWrapper({ children, title = "Inventory", className }: FullScreenWrapperProps) {
  const [isFullScreen, setIsFullScreen] = useState(false);

  const toggle = useCallback(() => setIsFullScreen((prev) => !prev), []);

  // Escape key to exit
  useEffect(() => {
    if (!isFullScreen) return;
    const handler = (e: KeyboardEvent) => {
      if (e.key === "Escape") setIsFullScreen(false);
    };
    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, [isFullScreen]);

  // Lock body scroll in full-screen
  useEffect(() => {
    if (isFullScreen) {
      document.body.style.overflow = "hidden";
    } else {
      document.body.style.overflow = "";
    }
    return () => { document.body.style.overflow = ""; };
  }, [isFullScreen]);

  return (
    <>
      {/* Toggle button */}
      <Button
        variant="outline"
        size="sm"
        onClick={toggle}
        className="no-print"
        title={isFullScreen ? "Exit full screen (Esc)" : "Full screen"}
      >
        {isFullScreen ? (
          <Minimize2 className="mr-1.5 h-3.5 w-3.5" />
        ) : (
          <Maximize2 className="mr-1.5 h-3.5 w-3.5" />
        )}
        {isFullScreen ? "Exit" : "Expand"}
      </Button>

      {/* Full-screen overlay */}
      <AnimatePresence>
        {isFullScreen && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            transition={springs.snappy}
            className="fixed inset-0 z-50 flex flex-col bg-white dark:bg-zinc-950"
          >
            {/* Top bar */}
            <div className="flex h-12 shrink-0 items-center justify-between border-b border-zinc-200/60 px-4 dark:border-zinc-700/60">
              <span className="text-sm font-semibold text-zinc-900 dark:text-zinc-50">
                {title}
              </span>
              <Button variant="ghost" size="sm" onClick={toggle}>
                <Minimize2 className="mr-1.5 h-3.5 w-3.5" />
                Exit Full Screen
                <kbd className="ml-2 text-[10px] bg-zinc-100 dark:bg-zinc-800 px-1.5 py-0.5 rounded">Esc</kbd>
              </Button>
            </div>
            {/* Content area */}
            <div className={cn("flex-1 overflow-auto p-4", className)}>
              {children}
            </div>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Normal flow content */}
      {!isFullScreen && (
        <div className={className}>
          {children}
        </div>
      )}
    </>
  );
}
