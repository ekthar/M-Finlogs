"use client";

import { useState, useEffect } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { Wifi, WifiOff, CloudOff, RefreshCw, CheckCircle2 } from "lucide-react";
import { getPendingCount } from "@/lib/offline-queue";

export function OnlineStatus() {
  const [online, setOnline] = useState(true);
  const [pendingCount, setPendingCount] = useState(0);
  const [justSynced, setJustSynced] = useState(false);

  useEffect(() => {
    setOnline(navigator.onLine);
    setPendingCount(getPendingCount());

    const on = () => { setOnline(true); };
    const off = () => { setOnline(false); };
    window.addEventListener("online", on);
    window.addEventListener("offline", off);

    // Listen for sync events
    const onSynced = () => {
      setPendingCount(getPendingCount());
      setJustSynced(true);
      setTimeout(() => setJustSynced(false), 3000);
    };
    window.addEventListener("mfinlogs:synced", onSynced);

    // Poll pending count every 5s
    const interval = setInterval(() => {
      setPendingCount(getPendingCount());
    }, 5000);

    return () => {
      window.removeEventListener("online", on);
      window.removeEventListener("offline", off);
      window.removeEventListener("mfinlogs:synced", onSynced);
      clearInterval(interval);
    };
  }, []);

  const showBanner = !online || pendingCount > 0 || justSynced;

  return (
    <AnimatePresence>
      {showBanner && (
        <motion.div
          initial={{ height: 0, opacity: 0 }}
          animate={{ height: "auto", opacity: 1 }}
          exit={{ height: 0, opacity: 0 }}
          transition={{ type: "spring", bounce: 0, duration: 0.3 }}
          className="overflow-hidden"
        >
          {!online ? (
            <div className="flex items-center justify-center gap-2 bg-amber-500 px-3 py-1.5 text-[11px] font-medium text-white">
              <WifiOff className="h-3.5 w-3.5" />
              You&apos;re offline
              {pendingCount > 0 && (
                <span className="ml-1 rounded-full bg-white/20 px-2 py-0.5">
                  {pendingCount} pending {pendingCount === 1 ? "entry" : "entries"}
                </span>
              )}
              <span className="opacity-75">&mdash; changes will sync when reconnected</span>
            </div>
          ) : justSynced ? (
            <div className="flex items-center justify-center gap-2 bg-emerald-500 px-3 py-1.5 text-[11px] font-medium text-white">
              <CheckCircle2 className="h-3.5 w-3.5" />
              All changes synced successfully
            </div>
          ) : pendingCount > 0 ? (
            <div className="flex items-center justify-center gap-2 bg-blue-500 px-3 py-1.5 text-[11px] font-medium text-white">
              <RefreshCw className="h-3.5 w-3.5 animate-spin" />
              Syncing {pendingCount} {pendingCount === 1 ? "entry" : "entries"}...
            </div>
          ) : null}
        </motion.div>
      )}
    </AnimatePresence>
  );
}
