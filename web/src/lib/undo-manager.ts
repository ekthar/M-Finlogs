"use client";

import { toast } from "sonner";

interface DeletedTransaction {
  txnId: number;
  data: Record<string, unknown>;
  deletedAt: number;
}

const UNDO_WINDOW = 10000; // 10 seconds to undo
let lastDeleted: DeletedTransaction | null = null;
let undoTimeout: ReturnType<typeof setTimeout> | null = null;

/**
 * Soft delete with undo — gives user 10 seconds to recover.
 * 
 * Flow:
 * 1. Mark as deleted (remove from UI)
 * 2. Show toast with "Undo" button
 * 3. If undo clicked within 10s → restore
 * 4. If timeout → permanently delete from server
 */
export async function softDelete(
  txnId: number,
  onRemoveFromUI: () => void,
  onRestoreToUI: () => void,
  onPermanentDelete: () => void
) {
  // Remove from UI immediately
  onRemoveFromUI();
  lastDeleted = { txnId, data: {}, deletedAt: Date.now() };

  // Clear any existing timeout
  if (undoTimeout) clearTimeout(undoTimeout);

  // Show toast with undo button
  toast("Transaction deleted", {
    description: "Click Undo to restore",
    action: {
      label: "Undo",
      onClick: () => {
        if (undoTimeout) clearTimeout(undoTimeout);
        lastDeleted = null;
        onRestoreToUI();
        toast.success("Restored");
      },
    },
    duration: UNDO_WINDOW,
  });

  // After 10s, permanently delete
  undoTimeout = setTimeout(async () => {
    if (lastDeleted && lastDeleted.txnId === txnId) {
      try {
        await fetch(`/api/transactions/${txnId}`, { method: "DELETE" });
        onPermanentDelete();
      } catch {
        // If delete fails, restore
        onRestoreToUI();
        toast.error("Delete failed — entry restored");
      }
      lastDeleted = null;
    }
  }, UNDO_WINDOW);
}
