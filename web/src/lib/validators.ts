import { z } from "zod";

/**
 * Data Validation Schemas
 * ─────────────────────────
 * Comprehensive validation with Indian business context:
 * - Amount: min ₹0.50, max ₹10 Crore (100,000,000)
 * - Dates: min April 2020, max today
 * - Party names: sanitized, no special SQL chars
 * - Bill numbers: alphanumeric + common separators
 */

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTANTS
// ═══════════════════════════════════════════════════════════════════════════════

const MIN_AMOUNT = 0.50;          // Minimum ₹0.50 (avoid dust transactions)
const MAX_AMOUNT = 100_000_000;   // Maximum ₹10 Crore
const MIN_DATE = new Date("2020-04-01"); // Oldest allowed date

// ═══════════════════════════════════════════════════════════════════════════════
// TRANSACTION SCHEMA
// ═══════════════════════════════════════════════════════════════════════════════

export const transactionSchema = z.object({
  txnDate: z.string()
    .min(1, "Date is required")
    .refine((val) => {
      const d = new Date(val);
      if (isNaN(d.getTime())) return false;
      // No future dates
      const today = new Date();
      today.setHours(23, 59, 59, 999);
      if (d > today) return false;
      // No dates before April 2020
      if (d < MIN_DATE) return false;
      return true;
    }, "Date must be between April 2020 and today"),
  billNo: z.string()
    .max(50, "Bill number too long")
    .regex(/^[a-zA-Z0-9\-\/\.\s]*$/, "Bill number contains invalid characters")
    .optional(),
  party: z.string()
    .min(1, "Party name is required")
    .max(100, "Party name too long")
    .refine((val) => {
      const trimmed = val.trim();
      if (!trimmed) return false;
      // Block dangerous SQL injection patterns only
      if (/--|;.*DROP|;.*DELETE|;.*INSERT|;.*UPDATE|;.*ALTER/i.test(trimmed)) return false;
      // Must have at least one letter (Latin, Devanagari, Bengali, etc.)
      if (!/[a-zA-Z\u0900-\u097F\u0980-\u09FF]/.test(trimmed)) return false;
      return true;
    }, "Invalid party name"),
  txnType: z.enum(["Sale", "Sale Return", "Expense", "Receipt", "Purchase"]),
  paymentMode: z.enum(["Cash", "Credit", "UPI", "Bank"]),
  amount: z.number()
    .min(MIN_AMOUNT, `Amount must be at least ₹${MIN_AMOUNT}`)
    .max(MAX_AMOUNT, `Amount cannot exceed ₹${MAX_AMOUNT.toLocaleString("en-IN")}`),
});

// ═══════════════════════════════════════════════════════════════════════════════
// PARTY SCHEMA
// ═══════════════════════════════════════════════════════════════════════════════

export const partySchema = z.object({
  name: z.string()
    .min(1, "Party name is required")
    .max(100, "Party name too long (max 100 chars)")
    .refine((val) => {
      const trimmed = val.trim();
      if (!trimmed) return false;
      if (/[;'"\\]|--/.test(trimmed)) return false;
      if (!/[a-zA-Z\u0900-\u097F]/.test(trimmed)) return false;
      return true;
    }, "Invalid party name — must contain letters, no special characters"),
  type: z.enum(["Customer", "Credit Customer", "Supplier", "Expense Account", "Bank"]),
  creditAllowed: z.boolean().default(false),
});

// ═══════════════════════════════════════════════════════════════════════════════
// AUTH SCHEMAS
// ═══════════════════════════════════════════════════════════════════════════════

export const loginSchema = z.object({
  username: z.string()
    .min(1, "Username is required")
    .max(50, "Username too long")
    .regex(/^[a-zA-Z0-9_]+$/, "Username: letters, numbers, underscore only"),
  password: z.string().min(1, "Password is required"),
});

export const createUserSchema = z.object({
  username: z.string()
    .min(3, "Username must be at least 3 characters")
    .max(30, "Username too long")
    .regex(/^[a-zA-Z0-9_]+$/, "Letters, numbers, underscore only"),
  password: z.string()
    .min(6, "Password must be at least 6 characters")
    .max(128, "Password too long"),
  role: z.enum(["admin", "accounts"]),
});

export const changePasswordSchema = z.object({
  username: z.string().min(1),
  newPassword: z.string().min(6, "Password must be at least 6 characters").max(128),
});

// ═══════════════════════════════════════════════════════════════════════════════
// OTHER SCHEMAS
// ═══════════════════════════════════════════════════════════════════════════════

export const companySchema = z.object({
  name: z.string()
    .min(1, "Company name is required")
    .max(100, "Company name too long"),
});

export const financialYearSchema = z.object({
  year: z.string().regex(/^\d{4}-\d{4}$/, "Format must be YYYY-YYYY"),
});

export const openingBalanceSchema = z.object({
  partyId: z.number().int().positive(),
  balance: z.number().min(-MAX_AMOUNT).max(MAX_AMOUNT),
  companyId: z.string().min(1),
});

export const cashInHandSchema = z.object({
  companyId: z.string().min(1),
  date: z.string().min(1),
  cashInHand: z.number().min(0).max(MAX_AMOUNT),
});

// ═══════════════════════════════════════════════════════════════════════════════
// TYPES
// ═══════════════════════════════════════════════════════════════════════════════

export type TransactionInput = z.infer<typeof transactionSchema>;
export type PartyInput = z.infer<typeof partySchema>;
export type LoginInput = z.infer<typeof loginSchema>;
export type CreateUserInput = z.infer<typeof createUserSchema>;
