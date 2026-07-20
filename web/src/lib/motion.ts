import type { Variants, Transition } from "framer-motion";

/**
 * Shared spring transitions following Apple's design principles:
 * - Critically damped (bounce: 0) for default UI
 * - response ~0.4s for smooth movement
 */
export const springTransition: Transition = {
  type: "spring" as const,
  bounce: 0,
  duration: 0.4,
};

export const springBounce: Transition = {
  type: "spring" as const,
  bounce: 0.2,
  duration: 0.4,
};

export const fadeUpVariant = {
  initial: { opacity: 0, y: 12 },
  animate: {
    opacity: 1,
    y: 0,
    transition: springTransition,
  },
};

export const staggerContainer: Variants = {
  initial: {},
  animate: {
    transition: {
      staggerChildren: 0.06,
    },
  },
};

export const staggerItem: Variants = {
  initial: { opacity: 0, y: 16 },
  animate: {
    opacity: 1,
    y: 0,
    transition: springTransition,
  },
};
