"use client";

import { usePathname } from "next/navigation";
import { TopNav } from "@/components/top-nav";
import { MobileNav } from "@/components/mobile-nav";
import { Toaster } from "@/components/ui/toast";
import { CommandPalette } from "@/components/ui/command-palette";
import { AppProvider } from "@/lib/app-context";
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

  return (
    <AppProvider>
      <div className="min-h-screen">
        <TopNav />
        <main
          className={cn(
            "mx-auto px-4 py-5 pb-20 lg:px-6 lg:pb-6",
            isFullWidth ? "max-w-none" : "max-w-7xl"
          )}
        >
          {children}
        </main>
        <MobileNav />
        <Toaster />
        <CommandPalette />
      </div>
    </AppProvider>
  );
}
