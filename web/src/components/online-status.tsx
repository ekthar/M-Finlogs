"use client";

import { useState, useEffect } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { Wifi, WifiOff } from "lucide-react";
import { springs } from "@/lib/design-tokens";

/**
 * Global online/offline status bar.
 * Shows a slim banner at the top when offline.
 */
export function OnlineStatus() {
  const [online, setOnline] = useState(true);

  useEffect(() => {
    setOnline(navigator.onLine);
    const on = () => setOnline(true);
    const off = () => setOnline(false);
    window.addEventListener("online", on);
    window.addEventListener("offline", off);
    return () => { window.removeEventListener("online", on); window.removeEventListener("offline", off); };
  }, []);

  return (
    <AnimatePresence>
      {!online && (
        <motion.div
          initial={{ height: 0, opacity: 0 }}
          animate={{ height: "auto", opacity: 1 }}
          exit={{ height: 0, opacity: 0 }}
          transition={springs.snappy}
          className="overflow-hidden"
        >
          <div className="flex items-center justify-center gap-2 bg-amber-500 px-3 py-1.5 text-[11px] font-medium text-white">
            <WifiOff className="h-3.5 w-3.5" />
            You&apos;re offline — changes will sync when reconnected
          </div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}
