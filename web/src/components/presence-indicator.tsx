"use client";

import { useState, useEffect } from "react";
import { useApp } from "@/lib/app-context";
import { usePathname } from "next/navigation";
import { Users } from "lucide-react";

interface OnlineUser { username: string; page: string; lastSeen: number; }

const pageNames: Record<string, string> = {
  "/": "Dashboard", "/daily-entry": "Daily Entry", "/reports/ledger": "Ledger",
  "/reports/day-book": "Day Book", "/reports/outstanding": "Outstanding",
  "/inventory": "Inventory", "/settings": "Settings", "/parties": "Parties",
};

/**
 * Shows who's currently online with their current page.
 * Sends heartbeat every 10s. Fetches presence list every 10s.
 */
export function PresenceIndicator() {
  const { username } = useApp();
  const pathname = usePathname();
  const [online, setOnline] = useState<OnlineUser[]>([]);
  const [showList, setShowList] = useState(false);

  useEffect(() => {
    const heartbeat = () => {
      fetch("/api/presence", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ username, page: pathname }),
      }).catch(() => {});
    };

    const fetchPresence = () => {
      fetch("/api/presence")
        .then(r => r.json())
        .then(d => setOnline(d.online || []))
        .catch(() => {});
    };

    heartbeat();
    fetchPresence();
    const hbInterval = setInterval(heartbeat, 10000);
    const fetchInterval = setInterval(fetchPresence, 10000);

    return () => { clearInterval(hbInterval); clearInterval(fetchInterval); };
  }, [username, pathname]);

  const others = online.filter(u => u.username !== username);
  if (others.length === 0) return null;

  return (
    <div className="relative">
      <button
        onClick={() => setShowList(!showList)}
        className="flex items-center gap-1.5 rounded-lg px-2 py-1 text-[11px] font-medium text-emerald-600 bg-emerald-50 dark:bg-emerald-900/20 dark:text-emerald-400 hover:bg-emerald-100 dark:hover:bg-emerald-900/30 transition-colors"
      >
        <span className="relative flex h-2 w-2">
          <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-emerald-400 opacity-75" />
          <span className="relative inline-flex rounded-full h-2 w-2 bg-emerald-500" />
        </span>
        <Users className="h-3 w-3" />
        {others.length} online
      </button>

      {showList && (
        <div className="absolute top-full right-0 mt-1 w-56 rounded-xl border border-zinc-200/60 bg-white/95 shadow-xl backdrop-blur-xl dark:border-zinc-700/60 dark:bg-zinc-900/95 z-50 p-2">
          <p className="px-2 py-1 text-[10px] font-semibold uppercase tracking-wider text-zinc-400">Online Now</p>
          {others.map(u => (
            <div key={u.username} className="flex items-center gap-2 px-2 py-1.5 rounded-lg">
              <div className="flex h-6 w-6 items-center justify-center rounded-full bg-gradient-to-br from-blue-500 to-indigo-600 text-[9px] font-bold text-white">
                {u.username.charAt(0).toUpperCase()}
              </div>
              <div className="min-w-0">
                <p className="text-xs font-medium text-zinc-800 dark:text-zinc-200 truncate">{u.username}</p>
                <p className="text-[10px] text-zinc-400">{pageNames[u.page] || u.page}</p>
              </div>
              <span className="ml-auto h-1.5 w-1.5 rounded-full bg-emerald-500" />
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
