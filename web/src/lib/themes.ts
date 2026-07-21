export interface Theme {
  key: string;
  name: string;
  description: string;
  preview: { bg: string; card: string; accent: string };
}

export const themes: Theme[] = [
  {
    key: "light",
    name: "Classic Light",
    description: "Clean white with subtle warmth",
    preview: { bg: "#f8f9fa", card: "#ffffff", accent: "#18181b" },
  },
  {
    key: "dark",
    name: "Dark",
    description: "Easy on the eyes at night",
    preview: { bg: "#09090b", card: "#18181b", accent: "#f4f4f5" },
  },
  {
    key: "deep-blue",
    name: "Deep Blue",
    description: "Professional navy (like the original app)",
    preview: { bg: "#0f172a", card: "#1e293b", accent: "#60a5fa" },
  },
  {
    key: "warm",
    name: "Warm Paper",
    description: "Cream tones, easy for long hours",
    preview: { bg: "#faf8f5", card: "#fffdf9", accent: "#92400e" },
  },
];

export function applyTheme(key: string) {
  const html = document.documentElement;
  // Remove all theme classes
  html.classList.remove("dark", "theme-deep-blue", "theme-warm");
  html.removeAttribute("data-theme");

  switch (key) {
    case "dark":
      html.classList.add("dark");
      break;
    case "deep-blue":
      html.classList.add("dark", "theme-deep-blue");
      html.setAttribute("data-theme", "deep-blue");
      break;
    case "warm":
      html.setAttribute("data-theme", "warm");
      break;
    default: // light
      break;
  }

  localStorage.setItem("mfinlogs_theme", key);
}

export function getStoredTheme(): string {
  if (typeof window === "undefined") return "light";
  return localStorage.getItem("mfinlogs_theme") || "light";
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Dark Mode Auto-Schedule (#10)
 * Automatically switches between dark/light themes based on time of day.
 * ───────────────────────────────────────────────────────────────────────────── */

export interface ThemeSchedule {
  enabled: boolean;
  darkStart: number; // hour 0-23
  darkEnd: number; // hour 0-23
}

export function getThemeSchedule(): ThemeSchedule {
  if (typeof window === "undefined") return { enabled: false, darkStart: 19, darkEnd: 7 };
  try {
    const stored = localStorage.getItem("mfinlogs_theme_schedule");
    return stored ? JSON.parse(stored) : { enabled: false, darkStart: 19, darkEnd: 7 };
  } catch { return { enabled: false, darkStart: 19, darkEnd: 7 }; }
}

export function setThemeSchedule(schedule: ThemeSchedule) {
  localStorage.setItem("mfinlogs_theme_schedule", JSON.stringify(schedule));
}

export function getScheduledTheme(): string | null {
  const schedule = getThemeSchedule();
  if (!schedule.enabled) return null;
  const hour = new Date().getHours();
  const { darkStart, darkEnd } = schedule;
  // Handle overnight range (e.g., 19-7 = dark from 7pm to 7am)
  if (darkStart > darkEnd) {
    return (hour >= darkStart || hour < darkEnd) ? "dark" : "light";
  }
  return (hour >= darkStart && hour < darkEnd) ? "dark" : "light";
}
