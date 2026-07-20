"use client";

import { Toaster as SonnerToaster } from "sonner";

export function Toaster() {
  return (
    <SonnerToaster
      position="bottom-center"
      expand={true}
      toastOptions={{
        style: {
          background: "rgba(255,255,255,0.92)",
          backdropFilter: "blur(16px) saturate(180%)",
          border: "1px solid rgba(0,0,0,0.08)",
          borderRadius: "14px",
          fontSize: "13px",
          boxShadow: "0 8px 32px rgba(0,0,0,0.12)",
          padding: "12px 16px",
        },
        className: "font-sans",
      }}
      richColors
      closeButton
    />
  );
}
