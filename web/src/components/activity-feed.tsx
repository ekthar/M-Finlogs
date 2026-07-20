"use client";

import { useState, useEffect } from "react";
import { useApp } from "@/lib/app-context";
import { Clock, User } from "lucide-react";

interface Activity { id: number; time: string; user: string; action: string; details: string | null; }

function timeAgo(dateStr: string): string {
  const diff = Date.now() - new Date(dateStr).getTime();
  const mins = Math.floor(diff / 60000);
  if (mins < 1) return "just now";
  if (mins < 60) return `${mins}m ago`;
  const hrs = Math.floor(mins / 60);
  if (hrs < 24) return `${hrs}h ago`;
  return `${Math.floor(hrs / 24)}d ago`;
}

/**
 * Activity Feed — shows recent actions as a timeline.
 * Polls every 15s for new entries.
 * Can be placed in a sidebar or card.
 */
export function ActivityFeed({ limit = 8 }: { limit?: number }) {
  const { companyId } = useApp();
  const [activities, setActivities] = useState<Activity[]>([]);

  useEffect(() => {
    const load = () => {
      fetch(`/api/activity?companyId=${companyId}&limit=${limit}`)
        .then(r => r.json())
        .then(d => setActivities(d.activities || []))
        .catch(() => {});
    };
    load();
    const interval = setInterval(load, 15000);
    return () => clearInterval(interval);
  }, [companyId, limit]);

  if (activities.length === 0) {
    return <p className="text-xs text-zinc-400 text-center py-4">No recent activity</p>;
  }

  return (
    <div className="space-y-1">
      {activities.map((a) => (
        <div key={a.id} className="flex items-start gap-2.5 px-1 py-1.5 rounded-lg hover:bg-zinc-50 dark:hover:bg-zinc-800/50 transition-colors">
          <div className="flex h-6 w-6 shrink-0 items-center justify-center rounded-full bg-zinc-100 dark:bg-zinc-800 mt-0.5">
            <User className="h-3 w-3 text-zinc-400" />
          </div>
          <div className="min-w-0 flex-1">
            <p className="text-[11px] text-zinc-700 dark:text-zinc-300 truncate">
              <span className="font-medium">{a.user}</span> {a.action.toLowerCase()}
            </p>
            {a.details && <p className="text-[10px] text-zinc-400 truncate">{a.details}</p>}
          </div>
          <span className="text-[10px] text-zinc-400 shrink-0 mt-0.5">{timeAgo(a.time)}</span>
        </div>
      ))}
    </div>
  );
}
