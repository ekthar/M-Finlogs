"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";
import { cn } from "@/lib/utils";
import { LayoutDashboard, PenLine, FileBarChart, Package, Menu } from "lucide-react";
import { useState } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { Sidebar } from "@/components/sidebar";

const bottomNav = [
  { label: "Home", href: "/", icon: LayoutDashboard },
  { label: "Entry", href: "/daily-entry", icon: PenLine },
  { label: "Reports", href: "/reports/ledger", icon: FileBarChart },
  { label: "Inventory", href: "/inventory", icon: Package },
];

export function MobileNav() {
  const pathname = usePathname();
  const [drawerOpen, setDrawerOpen] = useState(false);

  return (
    <>
      {/* Bottom Navigation Bar (mobile only) */}
      <nav className="fixed bottom-0 left-0 right-0 z-40 flex items-center justify-around border-t border-zinc-200/60 bg-white/80 backdrop-blur-xl px-2 py-1.5 lg:hidden dark:border-zinc-700/60 dark:bg-zinc-900/80">
        {bottomNav.map((item) => {
          const isActive = pathname === item.href || (item.href !== "/" && pathname.startsWith(item.href));
          return (
            <Link
              key={item.href}
              href={item.href}
              className={cn(
                "flex flex-col items-center gap-0.5 rounded-lg px-3 py-1.5 text-[10px] font-medium transition-colors",
                isActive ? "text-zinc-900 dark:text-white" : "text-zinc-400"
              )}
            >
              <item.icon className={cn("h-5 w-5", isActive && "text-zinc-900 dark:text-white")} />
              {item.label}
            </Link>
          );
        })}
        <button
          onClick={() => setDrawerOpen(true)}
          className="flex flex-col items-center gap-0.5 rounded-lg px-3 py-1.5 text-[10px] font-medium text-zinc-400"
        >
          <Menu className="h-5 w-5" />
          More
        </button>
      </nav>

      {/* Mobile Drawer */}
      <AnimatePresence>
        {drawerOpen && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            className="fixed inset-0 z-50 bg-black/40 backdrop-blur-sm lg:hidden"
            onClick={() => setDrawerOpen(false)}
          >
            <motion.div
              initial={{ x: -280 }}
              animate={{ x: 0 }}
              exit={{ x: -280 }}
              transition={{ type: "spring" as const, bounce: 0, duration: 0.35 }}
              className="h-full w-[280px]"
              onClick={(e) => e.stopPropagation()}
            >
              <Sidebar />
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>
    </>
  );
}
