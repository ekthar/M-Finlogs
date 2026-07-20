"use client";

import { TopNav } from "@/components/top-nav";
import { MobileNav } from "@/components/mobile-nav";
import { Toaster } from "@/components/ui/toast";
import { CommandPalette } from "@/components/ui/command-palette";
import { AppProvider } from "@/lib/app-context";

export default function DashboardLayout({
  children,
}: {
  children: React.ReactNode;
}) {
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
      </div>
    </AppProvider>
  );
}
