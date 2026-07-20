"use client";

import { useEffect, useRef } from "react";
import { toast } from "sonner";

const INACTIVITY_TIMEOUT = 30 * 60 * 1000; // 30 minutes
const WARNING_BEFORE = 5 * 60 * 1000; // Warn 5 min before

/**
 * Session Guard — warns user before session expires, auto-redirects on expiry.
 * Tracks mouse/keyboard activity. Resets timer on interaction.
 */
export function SessionGuard() {
  const timerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const warningRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const warned = useRef(false);

  useEffect(() => {
    const resetTimer = () => {
      warned.current = false;
      if (timerRef.current) clearTimeout(timerRef.current);
      if (warningRef.current) clearTimeout(warningRef.current);

      // Warning at 25 min
      warningRef.current = setTimeout(() => {
        warned.current = true;
        toast.warning("Session expiring in 5 minutes", {
          description: "Move your mouse or press a key to stay logged in",
          duration: 10000,
        });
      }, INACTIVITY_TIMEOUT - WARNING_BEFORE);

      // Logout at 30 min
      timerRef.current = setTimeout(() => {
        toast.error("Session expired — redirecting to login");
        setTimeout(() => {
          window.location.href = "/login";
        }, 2000);
      }, INACTIVITY_TIMEOUT);
    };

    const events = ["mousemove", "keydown", "click", "scroll", "touchstart"];
    events.forEach((e) => window.addEventListener(e, resetTimer, { passive: true }));
    resetTimer();

    return () => {
      events.forEach((e) => window.removeEventListener(e, resetTimer));
      if (timerRef.current) clearTimeout(timerRef.current);
      if (warningRef.current) clearTimeout(warningRef.current);
    };
  }, []);

  return null;
}
