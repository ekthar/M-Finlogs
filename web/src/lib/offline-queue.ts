"use client";

/**
 * Smart Transaction Queue — Fixed Sync Logic
 * 
 * Strategy: ALWAYS queue first (instant UI) → sync in background → deduplicate
 * 
 * Fixes applied:
 * - Race condition: syncRequested flag ensures re-trigger after completion
 * - Batch save: all status updates saved in ONE localStorage write (not per-entry)
 * - 400/validation errors → permanent "rejected" (no retry loop)
 * - 401 errors → pause sync (auth expired, user must re-login)
 * - 409 (duplicate) → mark as synced (already exists on server)
 * - Auto-sync interval reduced to 10s (was 15s)
 * - Initial sync after 500ms (was 1000ms)
 */

export interface QueuedTransaction {
  clientId: string;
  txnDate: string;
  billNo?: string;
  party: string;
  txnType: string;
  paymentMode: string;
  amount: number;
  companyId: string;
  queuedAt: number;
  status: "pending" | "syncing" | "synced" | "failed" | "rejected";
  retries: number;
  serverTxnId?: number;
  lastError?: string;
}

const QUEUE_KEY = "mfinlogs_txn_queue";
const MAX_RETRIES = 5;
let isSyncing = false;
let syncRequested = false;

function getQueue(): QueuedTransaction[] {
  try {
    return JSON.parse(localStorage.getItem(QUEUE_KEY) || "[]");
  } catch { return []; }
}

function saveQueue(queue: QueuedTransaction[]) {
  localStorage.setItem(QUEUE_KEY, JSON.stringify(queue));
}

/**
 * Add transaction to queue. Returns immediately (<1ms).
 */
export function enqueue(txn: Omit<QueuedTransaction, "clientId" | "queuedAt" | "status" | "retries">): QueuedTransaction {
  const entry: QueuedTransaction = {
    ...txn,
    clientId: `txn_${Date.now()}_${Math.random().toString(36).slice(2, 9)}`,
    queuedAt: Date.now(),
    status: "pending",
    retries: 0,
  };
  const queue = getQueue();
  queue.push(entry);
  saveQueue(queue);
  requestSync();
  return entry;
}

/**
 * Get count of entries not yet confirmed by server.
 * Only counts entries that are actively pending or failed with retries remaining.
 */
export function getPendingCount(): number {
  return getQueue().filter(q => 
    q.status === "pending" || 
    q.status === "syncing" || 
    (q.status === "failed" && q.retries < MAX_RETRIES)
  ).length;
}

/**
 * Get all pending entries (for showing in UI as "syncing")
 */
export function getPendingEntries(): QueuedTransaction[] {
  return getQueue().filter(q => q.status !== "synced" && q.status !== "rejected");
}

/**
 * Get rejected entries (validation errors — won't be retried)
 */
export function getRejectedEntries(): QueuedTransaction[] {
  return getQueue().filter(q => q.status === "rejected");
}

/**
 * Clear all synced entries from queue (cleanup).
 * Also removes entries older than 24 hours that have permanently failed.
 */
export function cleanupSynced() {
  const now = Date.now();
  const ONE_DAY = 24 * 60 * 60 * 1000;
  const queue = getQueue().filter(q => {
    // Remove synced entries
    if (q.status === "synced") return false;
    // Remove rejected entries (permanent failures)
    if (q.status === "rejected") return false;
    // Remove failed entries older than 24 hours with max retries exhausted
    if (q.status === "failed" && q.retries >= MAX_RETRIES && (now - q.queuedAt) > ONE_DAY) return false;
    return true;
  });
  saveQueue(queue);
}

/**
 * Remove a specific entry by clientId
 */
export function removeEntry(clientId: string) {
  const queue = getQueue().filter(q => q.clientId !== clientId);
  saveQueue(queue);
}

/**
 * Clear everything (for debugging / reset)
 */
export function clearQueue() {
  localStorage.removeItem(QUEUE_KEY);
}

/**
 * Non-blocking sync trigger.
 * If sync is already running, marks that another sync is needed after.
 */
function requestSync() {
  if (isSyncing) {
    syncRequested = true;
    return;
  }
  setTimeout(() => syncQueue(), 0);
}

/**
 * Core sync logic — processes queue entries one by one.
 * Fixed: proper error categorization, batch save, re-trigger on completion.
 */
async function syncQueue(): Promise<number> {
  if (isSyncing || !navigator.onLine) return 0;
  isSyncing = true;
  syncRequested = false;

  let synced = 0;

  // Read queue ONCE at start
  const queue = getQueue();
  const toSync = queue.filter(q =>
    (q.status === "pending" || q.status === "failed") && q.retries < MAX_RETRIES
  );

  for (const entry of toSync) {
    entry.status = "syncing";

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
        const data = await res.json();
        entry.status = "synced";
        entry.serverTxnId = data.transaction?.txnId;
        synced++;
      } else if (res.status === 409) {
        // Duplicate — already exists on server (success)
        entry.status = "synced";
        synced++;
      } else if (res.status === 400) {
        // Validation error — permanent rejection (won't retry)
        const errorData = await res.json().catch(() => ({ error: "Validation failed" }));
        entry.status = "rejected";
        entry.lastError = errorData.error || "Validation failed";
      } else if (res.status === 401) {
        // Auth expired — stop syncing entirely
        entry.status = "failed";
        entry.lastError = "Session expired";
        break;
      } else {
        // Server error — retry later
        entry.status = "failed";
        entry.retries++;
        entry.lastError = `Server error ${res.status}`;
      }
    } catch {
      entry.status = "failed";
      entry.lastError = "Network error";
    }
  }

  // Save ALL status updates in ONE write
  saveQueue(queue);
  isSyncing = false;

  // Notify UI
  if (synced > 0) {
    window.dispatchEvent(new CustomEvent("mfinlogs:synced", { detail: { synced } }));
  }

  // Re-trigger if sync was requested while busy, or if pending items remain
  if (syncRequested || getPendingCount() > 0) {
    setTimeout(() => requestSync(), 500);
  }

  return synced;
}

/**
 * Start auto-sync: retries every 10s + on reconnect.
 * Call once on app mount.
 */
export function startAutoSync() {
  if (typeof window === "undefined") return;

  const onOnline = () => { requestSync(); };
  window.addEventListener("online", onOnline);

  const interval = setInterval(() => {
    if (navigator.onLine && getPendingCount() > 0) {
      requestSync();
    }
  }, 10000);

  // Initial sync after 500ms
  setTimeout(requestSync, 500);

  return () => {
    window.removeEventListener("online", onOnline);
    clearInterval(interval);
  };
}
