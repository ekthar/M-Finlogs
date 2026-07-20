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
