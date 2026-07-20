"use client";

import { useEffect } from "react";
import { TopNav } from "@/components/top-nav";
import { MobileNav } from "@/components/mobile-nav";
import { Toaster } from "@/components/ui/toast";
import { CommandPalette } from "@/components/ui/command-palette";
import { SessionGuard } from "@/components/session-guard";
import { AppProvider } from "@/lib/app-context";
import { applyTheme, getStoredTheme } from "@/lib/themes";

export default function DashboardLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  // Apply stored theme on mount
  useEffect(() => {
    applyTheme(getStoredTheme());
  }, []);

  return (
    <AppProvider>
      <div className="min-h-screen">
        <TopNav />
        <main className="mx-auto max-w-7xl px-4 py-5 pb-20 lg:px-6 lg:pb-6">
          {children}
        </main>
        <MobileNav />
        <Toaster />
        <CommandPalette />
        <SessionGuard />
      </div>
    </AppProvider>
  );
}
