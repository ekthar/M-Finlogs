"use client";

/**
 * Offline Queue — saves transactions locally first, syncs to server when online.
 * 
 * Flow:
 * 1. User saves → entry goes to localStorage queue + shown in UI immediately
 * 2. Background worker attempts to POST to server
 * 3. On success → remove from queue
 * 4. On failure (offline) → stay in queue, retry on reconnect
 * 5. On page load → attempt to flush any pending items
 */

export interface QueuedTransaction {
  id: string; // temp client ID
  txnDate: string;
  billNo?: string;
  party: string;
  txnType: string;
  paymentMode: string;
  amount: number;
  companyId: string;
  timestamp: number;
  status: "pending" | "syncing" | "failed";
  retries: number;
}

const QUEUE_KEY = "mfinlogs_offline_queue";
const MAX_RETRIES = 5;

export function getQueue(): QueuedTransaction[] {
  try {
    const raw = localStorage.getItem(QUEUE_KEY);
    return raw ? JSON.parse(raw) : [];
  } catch { return []; }
}

function saveQueue(queue: QueuedTransaction[]) {
  localStorage.setItem(QUEUE_KEY, JSON.stringify(queue));
}

export function addToQueue(txn: Omit<QueuedTransaction, "id" | "timestamp" | "status" | "retries">): QueuedTransaction {
  const entry: QueuedTransaction = {
    ...txn,
    id: `local_${Date.now()}_${Math.random().toString(36).slice(2, 8)}`,
    timestamp: Date.now(),
    status: "pending",
    retries: 0,
  };
  const queue = getQueue();
  queue.push(entry);
  saveQueue(queue);
  return entry;
}

export function removeFromQueue(id: string) {
  const queue = getQueue().filter((q) => q.id !== id);
  saveQueue(queue);
}

export function markFailed(id: string) {
  const queue = getQueue().map((q) =>
    q.id === id ? { ...q, status: "failed" as const, retries: q.retries + 1 } : q
  );
  saveQueue(queue);
}

export function getPendingCount(): number {
  return getQueue().filter((q) => q.status === "pending" || q.status === "failed").length;
}

export function clearQueue() {
  localStorage.removeItem(QUEUE_KEY);
}

/**
 * Flush queue — attempt to sync all pending entries to server.
 * Returns number of successfully synced items.
 */
export async function flushQueue(): Promise<number> {
  const queue = getQueue();
  const pending = queue.filter((q) => (q.status === "pending" || q.status === "failed") && q.retries < MAX_RETRIES);
  let synced = 0;

  for (const entry of pending) {
    try {
      const res = await fetch("/api/transactions", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          txnDate: entry.txnDate,
          billNo: entry.billNo || undefined,
          party: entry.party,
          txnType: entry.txnType,
          paymentMode: entry.paymentMode,
          amount: entry.amount,
          companyId: entry.companyId,
        }),
      });

      if (res.ok) {
        removeFromQueue(entry.id);
        synced++;
      } else {
        markFailed(entry.id);
      }
    } catch {
      // Network error — mark failed, will retry later
      markFailed(entry.id);
    }
  }

  return synced;
}

/**
 * Listen for online event and auto-flush
 */
export function startAutoSync() {
  if (typeof window === "undefined") return;

  const flush = async () => {
    const count = getPendingCount();
    if (count > 0) {
      const synced = await flushQueue();
      if (synced > 0) {
        // Dispatch custom event so UI can refresh
        window.dispatchEvent(new CustomEvent("mfinlogs:synced", { detail: { synced } }));
      }
    }
  };

  window.addEventListener("online", flush);
  // Also try every 30 seconds
  const interval = setInterval(flush, 30000);
  // Initial flush on load
  setTimeout(flush, 2000);

  return () => {
    window.removeEventListener("online", flush);
    clearInterval(interval);
  };
}
