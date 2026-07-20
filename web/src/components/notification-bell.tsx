"use client";

import { useState, useEffect, useRef } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { useApp } from "@/lib/app-context";
import { Bell, AlertTriangle, Info, AlertCircle } from "lucide-react";
import { springs } from "@/lib/design-tokens";

interface Notification {
  id: string; type: string; title: string; description: string;
  severity: "info" | "warning" | "danger"; time: string;
}

const severityIcon = { info: Info, warning: AlertTriangle, danger: AlertCircle };
const severityColor = { info: "text-blue-500", warning: "text-amber-500", danger: "text-red-500" };
const severityBg = { info: "bg-blue-50 dark:bg-blue-900/20", warning: "bg-amber-50 dark:bg-amber-900/20", danger: "bg-red-50 dark:bg-red-900/20" };

export function NotificationBell() {
  const { companyId } = useApp();
  const [notifications, setNotifications] = useState<Notification[]>([]);
  const [open, setOpen] = useState(false);
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    fetch(`/api/notifications?companyId=${companyId}`)
      .then(r => r.json())
      .then(d => setNotifications(d.notifications || []))
      .catch(() => {});

    // Refresh every 5 minutes
    const interval = setInterval(() => {
      fetch(`/api/notifications?companyId=${companyId}`)
        .then(r => r.json())
        .then(d => setNotifications(d.notifications || []))
        .catch(() => {});
    }, 300000);
    return () => clearInterval(interval);
  }, [companyId]);

  // Close on click outside
  useEffect(() => {
    const handler = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) setOpen(false);
    };
    document.addEventListener("mousedown", handler);
    return () => document.removeEventListener("mousedown", handler);
  }, []);

  const count = notifications.length;

  return (
    <div ref={ref} className="relative">
      <button
        onClick={() => setOpen(!open)}
        className="relative flex h-8 w-8 items-center justify-center rounded-lg hover:bg-zinc-100 dark:hover:bg-zinc-800 transition-colors"
      >
        <Bell className="h-4 w-4 text-zinc-500" />
        {count > 0 && (
          <span className="absolute -top-0.5 -right-0.5 flex h-4 w-4 items-center justify-center rounded-full bg-red-500 text-[9px] font-bold text-white">
            {count > 9 ? "9+" : count}
          </span>
        )}
      </button>

      <AnimatePresence>
        {open && (
          <motion.div
            initial={{ opacity: 0, y: 4, scale: 0.96 }}
            animate={{ opacity: 1, y: 0, scale: 1 }}
            exit={{ opacity: 0, y: 4, scale: 0.96 }}
            transition={springs.snappy}
            className="absolute right-0 top-full mt-2 w-80 rounded-xl border border-zinc-200/60 bg-white/95 shadow-xl backdrop-blur-xl dark:border-zinc-700/60 dark:bg-zinc-900/95 z-50 overflow-hidden"
          >
            <div className="px-4 py-3 border-b border-zinc-100 dark:border-zinc-800">
              <p className="text-xs font-semibold text-zinc-900 dark:text-zinc-100">Notifications</p>
              <p className="text-[10px] text-zinc-400">{count} alert{count !== 1 ? "s" : ""}</p>
            </div>
            <div className="max-h-72 overflow-y-auto">
              {notifications.length === 0 ? (
                <div className="py-8 text-center text-xs text-zinc-400">All clear — no alerts</div>
              ) : (
                notifications.map((n) => {
                  const Icon = severityIcon[n.severity];
                  return (
                    <div key={n.id} className={`flex gap-3 px-4 py-3 border-b border-zinc-50 dark:border-zinc-800/50 last:border-0 ${severityBg[n.severity]}`}>
                      <Icon className={`h-4 w-4 mt-0.5 shrink-0 ${severityColor[n.severity]}`} />
                      <div className="min-w-0">
                        <p className="text-xs font-medium text-zinc-800 dark:text-zinc-200 truncate">{n.title}</p>
                        <p className="text-[10px] text-zinc-500 mt-0.5">{n.description}</p>
                      </div>
                    </div>
                  );
                })
              )}
            </div>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
}
