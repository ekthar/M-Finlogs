"use client";

import { Toaster as SonnerToaster } from "sonner";

export function Toaster() {
  return (
    <SonnerToaster
      position="top-right"
      toastOptions={{
        style: {
          background: "rgba(255,255,255,0.85)",
          backdropFilter: "blur(12px) saturate(180%)",
          border: "1px solid rgba(0,0,0,0.06)",
          borderRadius: "14px",
          fontSize: "13px",
          boxShadow: "0 8px 32px rgba(0,0,0,0.08)",
        },
        className: "font-sans",
      }}
      richColors
      closeButton
    />
  );
}
