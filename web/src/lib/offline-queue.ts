"use client";

/**
 * Smart Transaction Queue
 * 
 * Strategy: ALWAYS queue first (instant UI) → sync in background → deduplicate
 * 
 * Flow:
 * 1. User saves → entry added to queue with unique clientId → shown in UI INSTANTLY
 * 2. Sync worker picks up pending entries and POSTs them ONE BY ONE
 * 3. On success → mark as "synced" (remove from queue)
 * 4. On failure → mark as "failed", increment retry count
 * 5. Deduplication: each entry has a unique clientId. Server checks if already processed.
 * 
 * Why this is fast:
 * - handleAdd() returns in <1ms (just localStorage write + setState)
 * - No await on fetch — sync happens in background
 * - Even if user spams Enter, each gets a unique ID
 */

export interface QueuedTransaction {
  clientId: string; // Unique per entry — used for deduplication
  txnDate: string;
  billNo?: string;
  party: string;
  txnType: string;
  paymentMode: string;
  amount: number;
  companyId: string;
  queuedAt: number;
  status: "pending" | "syncing" | "synced" | "failed";
  retries: number;
  serverTxnId?: number; // Set after successful sync
}

const QUEUE_KEY = "mfinlogs_txn_queue";
const MAX_RETRIES = 10;
let isSyncing = false; // Prevent concurrent sync runs

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
  // Trigger sync immediately (non-blocking)
  requestSync();
  return entry;
}

/**
 * Get count of entries not yet confirmed by server
 */
export function getPendingCount(): number {
  return getQueue().filter(q => q.status === "pending" || q.status === "syncing" || q.status === "failed").length;
}

/**
 * Get all pending entries (for showing in UI as "syncing")
 */
export function getPendingEntries(): QueuedTransaction[] {
  return getQueue().filter(q => q.status !== "synced");
}

/**
 * Clear all synced entries from queue (cleanup)
 */
export function cleanupSynced() {
  const queue = getQueue().filter(q => q.status !== "synced");
  saveQueue(queue);
}

/**
 * Clear everything (for debugging / reset)
 */
export function clearQueue() {
  localStorage.removeItem(QUEUE_KEY);
}

/**
 * Non-blocking sync trigger. Runs sync in background.
 * Safe to call multiple times — uses lock to prevent concurrent runs.
 */
function requestSync() {
  if (isSyncing) return;
  setTimeout(() => syncQueue(), 0);
}

/**
 * Core sync logic — processes queue entries one by one.
 * Uses clientId for server-side deduplication.
 */
async function syncQueue(): Promise<number> {
  if (isSyncing || !navigator.onLine) return 0;
  isSyncing = true;

  let synced = 0;
  const queue = getQueue();
  const toSync = queue.filter(q => (q.status === "pending" || q.status === "failed") && q.retries < MAX_RETRIES);

  for (const entry of toSync) {
    // Mark as syncing
    updateEntryStatus(entry.clientId, "syncing");

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
          clientId: entry.clientId, // Server uses this for dedup
        }),
      });

      if (res.ok) {
        const data = await res.json();
        markSynced(entry.clientId, data.transaction?.txnId);
        synced++;
      } else if (res.status === 409) {
        // 409 = already exists (duplicate) — mark as synced
        markSynced(entry.clientId);
        synced++;
      } else {
        incrementRetry(entry.clientId);
      }
    } catch {
      // Network error — will retry later
      updateEntryStatus(entry.clientId, "failed");
    }
  }

  isSyncing = false;

  // Notify UI if any synced
  if (synced > 0) {
    window.dispatchEvent(new CustomEvent("mfinlogs:synced", { detail: { synced } }));
  }

  return synced;
}

function updateEntryStatus(clientId: string, status: QueuedTransaction["status"]) {
  const queue = getQueue().map(q => q.clientId === clientId ? { ...q, status } : q);
  saveQueue(queue);
}

function markSynced(clientId: string, serverTxnId?: number) {
  const queue = getQueue().map(q =>
    q.clientId === clientId ? { ...q, status: "synced" as const, serverTxnId } : q
  );
  saveQueue(queue);
}

function incrementRetry(clientId: string) {
  const queue = getQueue().map(q =>
    q.clientId === clientId ? { ...q, status: "failed" as const, retries: q.retries + 1 } : q
  );
  saveQueue(queue);
}

/**
 * Start auto-sync: retries every 15s + on reconnect.
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
  }, 15000);

  // Initial sync after 1s
  setTimeout(requestSync, 1000);

  return () => {
    window.removeEventListener("online", onOnline);
    clearInterval(interval);
  };
}
