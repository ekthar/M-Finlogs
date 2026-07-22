"use client";

import { useEffect } from "react";
import { usePathname } from "next/navigation";
import { TopNav } from "@/components/top-nav";
import { MobileNav } from "@/components/mobile-nav";
import { Toaster } from "@/components/ui/toast";
import { CommandPalette } from "@/components/ui/command-palette";
import { SessionGuard } from "@/components/session-guard";
import { ShortcutsPanel } from "@/components/shortcuts-panel";
import { Breadcrumbs } from "@/components/breadcrumbs";
import { OnlineStatus } from "@/components/online-status";
import { AppProvider } from "@/lib/app-context";
import { applyTheme, getStoredTheme, getScheduledTheme } from "@/lib/themes";
import { cn } from "@/lib/utils";

/** Routes that use full-width layout (no max-w constraint) */
const FULL_WIDTH_ROUTES = ["/inventory"];

export default function DashboardLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  const pathname = usePathname();
  const isFullWidth = FULL_WIDTH_ROUTES.some(
    (route) => pathname === route || pathname.startsWith(route + "/")
  );

  // Apply stored theme on mount (with schedule support)
  useEffect(() => {
    const scheduled = getScheduledTheme();
    applyTheme(scheduled || getStoredTheme());

    // Check every minute for theme schedule changes
    const interval = setInterval(() => {
      const next = getScheduledTheme();
      if (next) applyTheme(next);
    }, 60000);
    return () => clearInterval(interval);
  }, []);

  return (
    <AppProvider>
      <div className="min-h-screen">
        <OnlineStatus />
        <TopNav />
        <main
          className={cn(
            "mx-auto px-4 py-5 pb-20 lg:px-6 lg:pb-6",
            isFullWidth ? "max-w-none" : "max-w-7xl"
          )}
        >
          <Breadcrumbs />
          {children}
        </main>
        <MobileNav />
        <Toaster />
        <CommandPalette />
        <SessionGuard />
        <ShortcutsPanel />
      </div>
    </AppProvider>
  );
}
