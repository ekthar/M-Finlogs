import { z } from "zod";

export const transactionSchema = z.object({
  txnDate: z.string().regex(/^\d{4}-\d{2}-\d{2}$/),
  billNo: z.string().optional(),
  party: z.string().min(1, "Party is required"),
  txnType: z.enum(["Sale", "Sale Return", "Expense", "Receipt", "Purchase"]),
  paymentMode: z.enum(["Cash", "Credit", "UPI", "Bank"]),
  amount: z.number().positive("Amount must be positive"),
});

export const partySchema = z.object({
  name: z.string().min(1, "Party name is required"),
  type: z.enum(["Customer", "Credit Customer", "Supplier", "Expense Account", "Bank"]),
  creditAllowed: z.boolean().default(false),
});

export const loginSchema = z.object({
  username: z.string().min(1, "Username is required"),
  password: z.string().min(1, "Password is required"),
});

export const createUserSchema = z.object({
  username: z.string().min(1, "Username is required"),
  password: z.string().min(6, "Password must be at least 6 characters"),
  role: z.enum(["admin", "accounts"]),
});

export const changePasswordSchema = z.object({
  username: z.string().min(1),
  newPassword: z.string().min(6),
});

export const companySchema = z.object({
  name: z.string().min(1, "Company name is required"),
});

export const financialYearSchema = z.object({
  year: z.string().regex(/^\d{4}-\d{4}$/, "Format must be YYYY-YYYY"),
});

export type TransactionInput = z.infer<typeof transactionSchema>;
export type PartyInput = z.infer<typeof partySchema>;
export type LoginInput = z.infer<typeof loginSchema>;
export type CreateUserInput = z.infer<typeof createUserSchema>;
