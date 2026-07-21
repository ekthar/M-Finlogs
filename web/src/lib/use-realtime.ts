"use client";

import { useEffect, useRef, useCallback } from "react";
import { useApp } from "@/lib/app-context";

export type RealtimeEvent = {
  type: "transaction_created" | "transaction_updated" | "transaction_deleted" | "party_created" | "connected";
  payload?: any;
  sender?: string;
  timestamp?: number;
};

type EventHandler = (event: RealtimeEvent) => void;

/**
 * Hook for real-time collaboration via Server-Sent Events.
 * Automatically reconnects on disconnect.
 *
 * Usage:
 *   useRealtime((event) => {
 *     if (event.type === "transaction_created") refreshData();
 *   });
 */
export function useRealtime(onEvent: EventHandler) {
  const { companyId, username } = useApp();
  const eventSourceRef = useRef<EventSource | null>(null);
  const reconnectTimeout = useRef<NodeJS.Timeout | null>(null);

  useEffect(() => {
    if (!companyId || !username) return;

    function connect() {
      const url = `/api/events?companyId=${encodeURIComponent(companyId)}&username=${encodeURIComponent(username)}`;
      const es = new EventSource(url);
      eventSourceRef.current = es;

      es.onmessage = (event) => {
        try {
          const data: RealtimeEvent = JSON.parse(event.data);
          onEvent(data);
        } catch { /* ignore parse errors */ }
      };

      es.onerror = () => {
        es.close();
        // Reconnect after 5 seconds
        reconnectTimeout.current = setTimeout(connect, 5000);
      };
    }

    connect();

    return () => {
      eventSourceRef.current?.close();
      if (reconnectTimeout.current) clearTimeout(reconnectTimeout.current);
    };
  }, [companyId, username, onEvent]);
}

/**
 * Broadcast an event to other connected clients.
 * Call this after a successful mutation (create/update/delete).
 */
export async function broadcastEvent(
  companyId: string,
  type: RealtimeEvent["type"],
  payload: any,
  sender: string
) {
  try {
    await fetch("/api/events", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ companyId, type, payload, sender }),
    });
  } catch { /* non-critical, ignore */ }
}
