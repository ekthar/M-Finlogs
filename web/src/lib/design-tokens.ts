/**
 * Apple-level Design Tokens for M-Finlogs
 * 
 * Motion: Springs with damping ratio + response (not fixed durations)
 * Typography: Size-specific tracking and leading
 * Depth: Material weight encodes hierarchy
 */

// ─── SPRING PRESETS ─────────────────────────────────────────────
// Apple uses damping ratio + response. Framer Motion maps:
// bounce = 1 - dampingRatio (so bounce:0 = critically damped)
// duration ≈ response

export const springs = {
  // Default UI — critically damped, no overshoot
  default: { type: "spring" as const, bounce: 0, duration: 0.4 },
  // Snappy — for buttons, tabs, small movements
  snappy: { type: "spring" as const, bounce: 0, duration: 0.25 },
  // Gentle — for page transitions, large movements
  gentle: { type: "spring" as const, bounce: 0, duration: 0.5 },
  // Bouncy — ONLY for momentum-driven interactions (flick, throw)
  bouncy: { type: "spring" as const, bounce: 0.15, duration: 0.4 },
  // Sheet/Drawer — slight bounce, fast response
  sheet: { type: "spring" as const, bounce: 0.08, duration: 0.35 },
  // Micro — ultra fast for small indicators
  micro: { type: "spring" as const, bounce: 0, duration: 0.15 },
} as const;

// ─── STAGGER PRESETS ────────────────────────────────────────────
export const stagger = {
  fast: 0.04,    // Badges, pills
  default: 0.06, // Cards, rows
  slow: 0.08,    // Large items
} as const;

// ─── DEPTH / ELEVATION ─────────────────────────────────────────
export const elevation = {
  card: "shadow-sm",
  cardHover: "shadow-md",
  dropdown: "shadow-xl",
  modal: "shadow-2xl",
  toast: "shadow-lg",
} as const;

// ─── MATERIAL (backdrop-filter values) ─────────────────────────
export const materials = {
  // Thinnest — toasts, badges
  thin: "backdrop-blur-md bg-white/60 dark:bg-zinc-900/60",
  // Regular — cards, panels
  regular: "backdrop-blur-xl bg-white/70 dark:bg-zinc-900/70",
  // Thick — headers, navbars (structural)
  thick: "backdrop-blur-2xl bg-white/65 saturate-[1.8] dark:bg-zinc-900/65",
  // Ultra — modals, sheets (foreground, demands attention)
  ultra: "backdrop-blur-3xl bg-white/90 dark:bg-zinc-900/90",
} as const;

// ─── BORDER RADIUS ─────────────────────────────────────────────
export const radii = {
  sm: "rounded-lg",    // 8px — inputs, badges
  md: "rounded-xl",    // 12px — buttons, small cards
  lg: "rounded-2xl",   // 16px — cards, modals
  xl: "rounded-3xl",   // 24px — login card, hero
} as const;

// ─── Z-INDEX SCALE ─────────────────────────────────────────────
export const zIndex = {
  page: 0,
  card: 1,
  stickyHeader: 30,
  nav: 40,
  dropdown: 50,
  modal: 50,
  toast: 60,
  commandPalette: 70,
} as const;
