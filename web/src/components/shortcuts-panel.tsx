"use client";

import { useState, useEffect } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { X } from "lucide-react";

const shortcuts = [
  { key: "⌘ K", action: "Open search / command palette" },
  { key: "?", action: "Show this shortcuts panel" },
  { key: "Enter", action: "Navigate to next field (in entry form)" },
  { key: "Esc", action: "Close modal / dialog" },
  { key: "⌘ P", action: "Print current page" },
];

export function ShortcutsPanel() {
  const [open, setOpen] = useState(false);

  useEffect(() => {
    const handleKey = (e: KeyboardEvent) => {
      // Only trigger on "?" when not in an input
      if (e.key === "?" && !(e.target instanceof HTMLInputElement) && !(e.target instanceof HTMLTextAreaElement) && !(e.target instanceof HTMLSelectElement)) {
        e.preventDefault();
        setOpen((o) => !o);
      }
      if (e.key === "Escape") setOpen(false);
    };
    window.addEventListener("keydown", handleKey);
    return () => window.removeEventListener("keydown", handleKey);
  }, []);

  return (
    <AnimatePresence>
      {open && (
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          className="fixed inset-0 z-[65] flex items-center justify-center bg-black/30 backdrop-blur-sm"
          onClick={() => setOpen(false)}
        >
          <motion.div
            initial={{ opacity: 0, scale: 0.95, y: 10 }}
            animate={{ opacity: 1, scale: 1, y: 0 }}
            exit={{ opacity: 0, scale: 0.95, y: 10 }}
            transition={{ type: "spring" as const, bounce: 0, duration: 0.25 }}
            className="w-full max-w-sm rounded-2xl border border-zinc-200 bg-white p-5 shadow-2xl dark:border-zinc-700 dark:bg-zinc-900"
            onClick={(e) => e.stopPropagation()}
          >
            <div className="flex items-center justify-between mb-4">
              <h3 className="text-sm font-semibold text-zinc-900 dark:text-zinc-100">Keyboard Shortcuts</h3>
              <button onClick={() => setOpen(false)} className="flex h-7 w-7 items-center justify-center rounded-lg hover:bg-zinc-100 dark:hover:bg-zinc-800">
                <X className="h-4 w-4 text-zinc-400" />
              </button>
            </div>
            <div className="space-y-2">
              {shortcuts.map((s) => (
                <div key={s.key} className="flex items-center justify-between py-1.5">
                  <span className="text-xs text-zinc-600 dark:text-zinc-400">{s.action}</span>
                  <kbd className="rounded-md bg-zinc-100 px-2 py-1 text-[11px] font-medium text-zinc-700 dark:bg-zinc-800 dark:text-zinc-300">{s.key}</kbd>
                </div>
              ))}
            </div>
            <p className="mt-4 text-center text-[10px] text-zinc-400">Press <kbd className="rounded bg-zinc-100 px-1 py-0.5 dark:bg-zinc-800">?</kbd> to toggle</p>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}
