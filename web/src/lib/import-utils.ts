/**
 * Smart Import Utilities
 * ─────────────────────
 * - Date parsing (Indian formats, DD.MM.YY, DD/MM/YYYY, etc.)
 * - Party name cleaning (/BLNC removal, whitespace normalization)
 * - Typo correction via Levenshtein distance
 * - Transaction type / payment mode normalization
 */

// ═══════════════════════════════════════════════════════════════════════════════
// DATE PARSING — handles every Indian business format
// ═══════════════════════════════════════════════════════════════════════════════

const DATE_FORMATS = [
  // DD.MM.YY → "17.01.26"
  /^(\d{1,2})[.\/-](\d{1,2})[.\/-](\d{2})$/,
  // DD.MM.YYYY → "17.01.2026"
  /^(\d{1,2})[.\/-](\d{1,2})[.\/-](\d{4})$/,
  // YYYY-MM-DD (ISO)
  /^(\d{4})-(\d{1,2})-(\d{1,2})$/,
];

/**
 * Parse date string with dayfirst=true logic (Indian format).
 * Returns Date or null if unparseable.
 */
export function smartParseDate(input: unknown): Date | null {
  if (!input) return null;
  const raw = String(input).trim();
  if (!raw) return null;

  // Try ISO first (YYYY-MM-DD)
  const isoMatch = /^(\d{4})-(\d{1,2})-(\d{1,2})/.exec(raw);
  if (isoMatch) {
    const d = new Date(parseInt(isoMatch[1]), parseInt(isoMatch[2]) - 1, parseInt(isoMatch[3]));
    if (isValidDate(d)) return d;
  }

  // Try DD.MM.YY or DD/MM/YY or DD-MM-YY
  const shortMatch = /^(\d{1,2})[.\/-](\d{1,2})[.\/-](\d{2})$/.exec(raw);
  if (shortMatch) {
    let year = parseInt(shortMatch[3]);
    year = year < 50 ? 2000 + year : 1900 + year; // 26 → 2026, 95 → 1995
    const day = parseInt(shortMatch[1]);
    const month = parseInt(shortMatch[2]);
    const d = new Date(year, month - 1, day);
    if (isValidDate(d)) return d;
  }

  // Try DD.MM.YYYY or DD/MM/YYYY or DD-MM-YYYY (dayfirst)
  const longMatch = /^(\d{1,2})[.\/-](\d{1,2})[.\/-](\d{4})$/.exec(raw);
  if (longMatch) {
    const day = parseInt(longMatch[1]);
    const month = parseInt(longMatch[2]);
    const year = parseInt(longMatch[3]);
    const d = new Date(year, month - 1, day);
    if (isValidDate(d)) return d;
  }

  // Try Excel serial number (days since 1900-01-01)
  const numVal = Number(raw);
  if (Number.isFinite(numVal) && numVal > 40000 && numVal < 60000) {
    // Excel serial date
    const d = new Date((numVal - 25569) * 86400000);
    if (isValidDate(d)) return d;
  }

  // Fallback: JavaScript Date constructor (handles ISO, RFC, etc.)
  const fallback = new Date(raw);
  if (isValidDate(fallback)) return fallback;

  return null;
}

function isValidDate(d: Date): boolean {
  return d instanceof Date && !isNaN(d.getTime()) && d.getFullYear() > 1990 && d.getFullYear() < 2100;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PARTY NAME CLEANING
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Clean party name:
 * - Strip /BLNC, /blnc, /balance, /BAL, /old, /new, etc.
 * - Remove trailing numbers like "Party Name 123"
 * - Collapse whitespace
 * - Title-case
 */
export function cleanPartyName(raw: unknown): string {
  if (!raw) return "";
  let name = String(raw).trim();

  // Remove /BLNC, /balance, /BAL, /old, /new, /2, etc.
  name = name.replace(/\s*\/\s*(blnc|balance|bal|old|new|\d+)\s*$/i, "");

  // Remove trailing balance amounts like "Name 5000" or "Name -2500"
  name = name.replace(/\s+[-]?\d{3,}$/, "");

  // Remove common suffixes people add
  name = name.replace(/\s*\(.*?\)\s*$/, ""); // (anything in brackets)
  name = name.replace(/\s*[-–—]\s*(balance|blnc|bal|due|pending)\s*$/i, "");

  // Collapse multiple spaces
  name = name.replace(/\s+/g, " ").trim();

  // Don't title-case if already mixed case (e.g., "McDonald's")
  if (name === name.toUpperCase() || name === name.toLowerCase()) {
    name = name
      .toLowerCase()
      .split(" ")
      .map((w) => w.charAt(0).toUpperCase() + w.slice(1))
      .join(" ");
  }

  return name;
}

// ═══════════════════════════════════════════════════════════════════════════════
// TYPO CORRECTION — Levenshtein distance matching
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Levenshtein distance between two strings.
 */
function levenshtein(a: string, b: string): number {
  const la = a.length;
  const lb = b.length;
  if (la === 0) return lb;
  if (lb === 0) return la;

  const matrix: number[][] = [];
  for (let i = 0; i <= la; i++) matrix[i] = [i];
  for (let j = 0; j <= lb; j++) matrix[0][j] = j;

  for (let i = 1; i <= la; i++) {
    for (let j = 1; j <= lb; j++) {
      const cost = a[i - 1] === b[j - 1] ? 0 : 1;
      matrix[i][j] = Math.min(
        matrix[i - 1][j] + 1,
        matrix[i][j - 1] + 1,
        matrix[i - 1][j - 1] + cost
      );
    }
  }
  return matrix[la][lb];
}

/**
 * Find the best matching party name from existing list.
 * Returns the matched name if similarity > threshold, else the cleaned input.
 * 
 * @param input - The raw party name from import
 * @param existingParties - List of existing party names
 * @param threshold - Max Levenshtein distance (default: 2 for short names, 3 for longer)
 */
export function matchPartyName(input: string, existingParties: string[]): { matched: string; isExact: boolean; corrected: boolean } {
  const cleaned = cleanPartyName(input);
  if (!cleaned) return { matched: "", isExact: false, corrected: false };

  const lower = cleaned.toLowerCase();

  // Exact match (case-insensitive)
  for (const p of existingParties) {
    if (p.toLowerCase() === lower) {
      return { matched: p, isExact: true, corrected: false };
    }
  }

  // Fuzzy match using Levenshtein
  const maxDist = cleaned.length <= 5 ? 1 : cleaned.length <= 10 ? 2 : 3;
  let bestMatch = "";
  let bestDist = Infinity;

  for (const p of existingParties) {
    const dist = levenshtein(lower, p.toLowerCase());
    if (dist < bestDist && dist <= maxDist) {
      bestDist = dist;
      bestMatch = p;
    }
  }

  if (bestMatch) {
    return { matched: bestMatch, isExact: false, corrected: true };
  }

  // No match found — return cleaned name as-is (will be auto-created)
  return { matched: cleaned, isExact: false, corrected: false };
}

// ═══════════════════════════════════════════════════════════════════════════════
// TRANSACTION TYPE NORMALIZATION
// ═══════════════════════════════════════════════════════════════════════════════

const TXN_TYPE_MAP: Record<string, string> = {
  // Sale variants
  sale: "Sale", sl: "Sale", "sl:l": "Sale", sales: "Sale", sael: "Sale",
  sell: "Sale", sold: "Sale", s: "Sale",
  // Expense variants
  expense: "Expense", exp: "Expense", expence: "Expense", expnse: "Expense",
  expenses: "Expense", e: "Expense",
  // Receipt variants
  receipt: "Receipt", rcpt: "Receipt", reciept: "Receipt", recept: "Receipt",
  receive: "Receipt", received: "Receipt", collection: "Receipt",
  col: "Receipt", coll: "Receipt", r: "Receipt",
  // Purchase variants
  purchase: "Purchase", pur: "Purchase", prchase: "Purchase", buy: "Purchase",
  purch: "Purchase", p: "Purchase",
  // Return variants
  "sale return": "Sale Return", "sales return": "Sale Return",
  return: "Sale Return", ret: "Sale Return", "s/r": "Sale Return",
  "sr": "Sale Return",
};

/**
 * Normalize transaction type with typo correction.
 * Returns standardized type or null if completely unrecognizable.
 */
export function normalizeTransactionType(raw: unknown): string {
  if (!raw) return "Sale";
  const key = String(raw).trim().toLowerCase().replace(/[^a-z/: ]/g, "");
  if (!key) return "Sale";

  // Direct lookup
  if (TXN_TYPE_MAP[key]) return TXN_TYPE_MAP[key];

  // Fuzzy match against known keys
  let bestMatch = "Sale";
  let bestDist = Infinity;
  for (const [k, v] of Object.entries(TXN_TYPE_MAP)) {
    const dist = levenshtein(key, k);
    if (dist < bestDist && dist <= 2) {
      bestDist = dist;
      bestMatch = v;
    }
  }

  return bestMatch;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PAYMENT MODE NORMALIZATION
// ═══════════════════════════════════════════════════════════════════════════════

const PAYMENT_MODE_MAP: Record<string, string> = {
  // Cash variants
  cash: "Cash", csh: "Cash", "c": "Cash", cah: "Cash", cas: "Cash",
  // Credit variants
  credit: "Credit", crdt: "Credit", cr: "Credit", cred: "Credit",
  credi: "Credit", credt: "Credit",
  // UPI variants
  upi: "UPI", gpay: "UPI", phonepe: "UPI", paytm: "UPI",
  googlepay: "UPI", "google pay": "UPI", online: "UPI",
  // Bank variants
  bank: "Bank", neft: "Bank", rtgs: "Bank", imps: "Bank",
  transfer: "Bank", bnk: "Bank", cheque: "Bank", check: "Bank",
  // Card variants
  card: "UPI", debit: "UPI", "debit card": "UPI", "credit card": "UPI",
};

/**
 * Normalize payment mode with typo correction.
 */
export function normalizePaymentMode(raw: unknown): string {
  if (!raw) return "Cash";
  const key = String(raw).trim().toLowerCase().replace(/[^a-z ]/g, "");
  if (!key) return "Cash";

  // Direct lookup
  if (PAYMENT_MODE_MAP[key]) return PAYMENT_MODE_MAP[key];

  // Fuzzy match
  let bestMatch = "Cash";
  let bestDist = Infinity;
  for (const [k, v] of Object.entries(PAYMENT_MODE_MAP)) {
    const dist = levenshtein(key, k);
    if (dist < bestDist && dist <= 2) {
      bestDist = dist;
      bestMatch = v;
    }
  }

  return bestMatch;
}

// ═══════════════════════════════════════════════════════════════════════════════
// COLUMN HEADER DETECTION — auto-map headers to known fields
// ═══════════════════════════════════════════════════════════════════════════════

export interface ColumnMapping {
  date: number;
  billNo: number;
  party: number;
  txnType: number;
  paymentMode: number;
  amount: number;
}

const HEADER_ALIASES: Record<keyof ColumnMapping, string[]> = {
  date: ["date", "txn_date", "txndate", "transaction date", "txn date", "dt"],
  billNo: ["bill no", "billno", "bill_no", "bill no / invoice", "invoice", "bill", "voucher", "vch"],
  party: ["party", "customer name", "name", "customer", "party name", "account", "client"],
  txnType: ["type", "txn_type", "txntype", "transaction type", "txn type", "tr type"],
  paymentMode: ["mode", "payment mode", "payment_mode", "paymentmode", "pay mode", "payment", "yment mo"],
  amount: ["amount", "amt", "total", "value", "sum"],
};

/**
 * Auto-detect column mapping from header row.
 * Returns mapping with column indices (-1 if not found).
 */
export function detectColumns(headers: string[]): ColumnMapping {
  const normalized = headers.map((h) => String(h || "").trim().toLowerCase());
  const mapping: ColumnMapping = { date: -1, billNo: -1, party: -1, txnType: -1, paymentMode: -1, amount: -1 };

  for (const [field, aliases] of Object.entries(HEADER_ALIASES)) {
    for (let i = 0; i < normalized.length; i++) {
      if (aliases.some((alias) => normalized[i] === alias || normalized[i].includes(alias))) {
        (mapping as any)[field] = i;
        break;
      }
    }
  }

  // Heuristic: if no date column found but first col looks like dates, use it
  if (mapping.date === -1 && normalized.length > 0) {
    mapping.date = 0;
  }

  // Heuristic: if no amount found, use last numeric-looking column
  if (mapping.amount === -1) {
    for (let i = normalized.length - 1; i >= 0; i--) {
      if (normalized[i].includes("amount") || normalized[i].includes("total") || normalized[i].includes("amt")) {
        mapping.amount = i;
        break;
      }
    }
  }

  return mapping;
}

// ═══════════════════════════════════════════════════════════════════════════════
// SMART IMPORT — Daily Register format detection (COL / SL:L)
// ═══════════════════════════════════════════════════════════════════════════════

export interface SmartImportRow {
  billNo: string;
  type: "Receipt" | "Sale";
  date: Date;
  party: string;
  paymentMode: string;
  amount: number;
}

/**
 * Detect if a file is in "daily register" format (COL/SL:L columns).
 * Returns parsed transactions split by payment mode.
 */
export function parseDailyRegister(rows: unknown[][]): SmartImportRow[] {
  if (!rows || rows.length < 3) return [];

  // Skip first 2 header rows, look for COL/SL:L in column B (index 1)
  const results: SmartImportRow[] = [];

  for (let i = 2; i < rows.length; i++) {
    const row = rows[i];
    if (!Array.isArray(row) || row.length < 8) continue;

    const typeRaw = String(row[1] || "").trim().toUpperCase();
    if (typeRaw !== "COL" && typeRaw !== "SL:L") continue;

    const billNo = String(row[0] || "").trim();
    const dateVal = smartParseDate(row[2]);
    if (!dateVal) continue;

    const partyRaw = String(row[3] || "").trim();
    const party = cleanPartyName(partyRaw);
    if (!party) continue;

    const saleOrCol = parseFloat(String(row[4] || "0").replace(/,/g, "")) || 0;
    const creditAmt = parseFloat(String(row[5] || "0").replace(/,/g, "")) || 0;
    const cashAmt = parseFloat(String(row[6] || "0").replace(/,/g, "")) || 0;
    const cardAmt = parseFloat(String(row[7] || "0").replace(/,/g, "")) || 0;

    if (saleOrCol <= 0) continue;

    if (typeRaw === "COL") {
      // Collection = Receipt. Use cash or card amount.
      const amount = cashAmt > 0 ? cashAmt : cardAmt;
      const mode = cashAmt > 0 ? "Cash" : "UPI";
      if (amount > 0) {
        results.push({ billNo, type: "Receipt", date: dateVal, party, paymentMode: mode, amount });
      }
    } else {
      // SL:L = Sale. Split into multiple transactions by mode.
      if (creditAmt > 0) {
        results.push({ billNo, type: "Sale", date: dateVal, party, paymentMode: "Credit", amount: creditAmt });
      }
      if (cashAmt > 0) {
        results.push({ billNo, type: "Sale", date: dateVal, party: "Customer", paymentMode: "Cash", amount: cashAmt });
      }
      if (cardAmt > 0) {
        results.push({ billNo, type: "Sale", date: dateVal, party: "Customer", paymentMode: "UPI", amount: cardAmt });
      }
    }
  }

  return results;
}

/**
 * Detect if data looks like a daily register format.
 */
export function isDailyRegisterFormat(rows: unknown[][]): boolean {
  if (!rows || rows.length < 4) return false;
  // Check if any row in first 10 has COL or SL:L in column B (index 1)
  for (let i = 0; i < Math.min(rows.length, 15); i++) {
    const row = rows[i];
    if (!Array.isArray(row)) continue;
    const val = String(row[1] || "").trim().toUpperCase();
    if (val === "COL" || val === "SL:L") return true;
  }
  return false;
}
