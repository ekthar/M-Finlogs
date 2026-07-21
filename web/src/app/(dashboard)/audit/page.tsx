"use client";

import { useEffect, useState } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { useApp } from "@/lib/app-context";
import { Button } from "@/components/ui/button";
import {
  Table,
  TableHeader,
  TableBody,
  TableRow,
  TableHead,
  TableCell,
} from "@/components/ui/data-table";
import { Badge } from "@/components/ui/badge";
import {
  RefreshCw,
  ChevronDown,
  ChevronRight,
  Minus,
  Plus,
} from "lucide-react";

/* ─────────────────────────────────────────────────────────────────────────────
 * Types
 * ───────────────────────────────────────────────────────────────────────────── */

interface LogEntry {
  id: number;
  timestamp: string;
  username: string;
  action: string;
  details: string | null;
}

interface DiffField {
  field: string;
  before: any;
  after: any;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Animation Presets
 * ───────────────────────────────────────────────────────────────────────────── */

const fadeUp = {
  initial: { opacity: 0, y: 12 },
  animate: {
    opacity: 1,
    y: 0,
    transition: { type: "spring" as const, bounce: 0, duration: 0.4 },
  },
};

/* ─────────────────────────────────────────────────────────────────────────────
 * Diff Parser
 * ───────────────────────────────────────────────────────────────────────────── */

function parseDiff(details: string | null): DiffField[] | null {
  if (!details) return null;
  try {
    const data = JSON.parse(details);
    // Support format: { before: {...}, after: {...} }
    if (data.before && data.after) {
      const fields: DiffField[] = [];
      const allKeys = new Set([
        ...Object.keys(data.before),
        ...Object.keys(data.after),
      ]);
      for (const key of allKeys) {
        const before = data.before[key];
        const after = data.after[key];
        if (JSON.stringify(before) !== JSON.stringify(after)) {
          fields.push({ field: key, before, after });
        }
      }
      return fields.length > 0 ? fields : null;
    }
    // Support format: { field: value, ... } (simple details)
    return null;
  } catch {
    return null;
  }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Diff Viewer — Clean grid layout with visible separator lines
 * ───────────────────────────────────────────────────────────────────────────── */

function DiffViewer({ diff }: { diff: DiffField[] }) {
  return (
    <div className="mt-3 overflow-hidden rounded-xl border border-zinc-200 dark:border-zinc-700 bg-white dark:bg-zinc-900/60 shadow-sm">
      {/* Header row */}
      <div className="grid grid-cols-[120px_1fr_1fr] border-b border-zinc-100 dark:border-zinc-800 bg-zinc-50/80 dark:bg-zinc-800/40 px-4 py-2">
        <span className="text-[10px] font-semibold uppercase tracking-wider text-zinc-400">
          Field
        </span>
        <span className="text-[10px] font-semibold uppercase tracking-wider text-red-400">
          Before
        </span>
        <span className="text-[10px] font-semibold uppercase tracking-wider text-emerald-500">
          After
        </span>
      </div>

      {/* Diff rows with visible divider lines */}
      {diff.map((d, i) => (
        <div
          key={d.field}
          className={`grid grid-cols-[120px_1fr_1fr] items-center gap-x-3 px-4 py-2.5 ${
            i !== diff.length - 1
              ? "border-b border-zinc-100 dark:border-zinc-800"
              : ""
          }`}
        >
          <span className="text-xs font-medium text-zinc-600 dark:text-zinc-300 truncate">
            {d.field}
          </span>

          <div className="flex items-center gap-1.5 min-w-0">
            <Minus className="h-3 w-3 text-red-400 flex-shrink-0" />
            <span className="text-xs text-red-600 dark:text-red-400 bg-red-50 dark:bg-red-900/20 px-2 py-0.5 rounded-md break-all font-mono">
              {String(d.before ?? "null")}
            </span>
          </div>

          <div className="flex items-center gap-1.5 min-w-0">
            <Plus className="h-3 w-3 text-emerald-500 flex-shrink-0" />
            <span className="text-xs text-emerald-600 dark:text-emerald-400 bg-emerald-50 dark:bg-emerald-900/20 px-2 py-0.5 rounded-md break-all font-mono">
              {String(d.after ?? "null")}
            </span>
          </div>
        </div>
      ))}
    </div>
  );
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Audit Row — Expandable with diff inline
 * ───────────────────────────────────────────────────────────────────────────── */

function AuditRow({ log }: { log: LogEntry }) {
  const [expanded, setExpanded] = useState(false);
  const diff = parseDiff(log.details);
  const hasExpandable = diff || (log.details && log.details.length > 50);

  const actionColor =
    log.action.toLowerCase().includes("delete")
      ? "danger"
      : log.action.toLowerCase().includes("create")
        ? "success"
        : log.action.toLowerCase().includes("update") ||
            log.action.toLowerCase().includes("edit")
          ? "info"
          : "default";

  return (
    <>
      <TableRow
        className={
          hasExpandable
            ? "cursor-pointer hover:bg-zinc-50 dark:hover:bg-zinc-800/50 transition-colors"
            : ""
        }
        onClick={() => hasExpandable && setExpanded(!expanded)}
      >
        <TableCell className="text-xs whitespace-nowrap text-zinc-500">
          {new Date(log.timestamp).toLocaleString("en-IN", {
            day: "2-digit",
            month: "short",
            hour: "2-digit",
            minute: "2-digit",
          })}
        </TableCell>
        <TableCell className="font-medium text-sm text-zinc-800 dark:text-zinc-200">
          {log.username}
        </TableCell>
        <TableCell>
          <Badge variant={actionColor} className="text-[10px]">
            {log.action}
          </Badge>
        </TableCell>
        <TableCell className="max-w-xs">
          <div className="flex items-center gap-1.5">
            {hasExpandable &&
              (expanded ? (
                <ChevronDown className="h-3.5 w-3.5 text-zinc-400 flex-shrink-0" />
              ) : (
                <ChevronRight className="h-3.5 w-3.5 text-zinc-400 flex-shrink-0" />
              ))}
            <span className="truncate text-zinc-500 text-xs">
              {diff
                ? `${diff.length} field${diff.length > 1 ? "s" : ""} changed`
                : log.details || "\u2014"}
            </span>
          </div>
        </TableCell>
      </TableRow>

      {expanded && hasExpandable && (
        <TableRow>
          <TableCell colSpan={4} className="py-0 px-4 pb-4">
            <AnimatePresence>
              <motion.div
                initial={{ opacity: 0, height: 0 }}
                animate={{ opacity: 1, height: "auto" }}
                exit={{ opacity: 0, height: 0 }}
              >
                {diff ? (
                  <DiffViewer diff={diff} />
                ) : (
                  <pre className="mt-3 rounded-xl border border-zinc-200 dark:border-zinc-700 p-4 bg-zinc-50 dark:bg-zinc-900/60 text-xs text-zinc-600 dark:text-zinc-400 whitespace-pre-wrap overflow-x-auto shadow-sm">
                    {log.details}
                  </pre>
                )}
              </motion.div>
            </AnimatePresence>
          </TableCell>
        </TableRow>
      )}
    </>
  );
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Audit Page — Main View
 * ───────────────────────────────────────────────────────────────────────────── */

export default function AuditPage() {
  const { companyId } = useApp();
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const [loading, setLoading] = useState(true);
  const [filter, setFilter] = useState<string>("all");

  const load = () => {
    setLoading(true);
    fetch(`/api/audit?companyId=${companyId}&limit=200`)
      .then((r) => r.json())
      .then((d) => setLogs(d.logs || []))
      .catch(() => {})
      .finally(() => setLoading(false));
  };

  useEffect(() => {
    load();
  }, [companyId]);

  const filtered =
    filter === "all"
      ? logs
      : logs.filter((l) => l.action.toLowerCase().includes(filter));

  const actions = [...new Set(logs.map((l) => l.action))];

  return (
    <motion.div initial="initial" animate="animate" className="space-y-6">
      {/* ─── Header ─── */}
      <motion.div
        {...fadeUp}
        className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-3"
      >
        <div>
          <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">
            Audit Logs
          </h1>
          <p className="mt-0.5 text-sm text-zinc-500">
            {filtered.length} records{" "}
            {filter !== "all" && (
              <span className="text-zinc-400">(filtered: {filter})</span>
            )}
          </p>
        </div>

        <div className="flex items-center gap-2">
          <select
            value={filter}
            onChange={(e) => setFilter(e.target.value)}
            className="h-8 rounded-lg border border-zinc-200 bg-white px-3 text-xs font-medium
                       dark:border-zinc-700 dark:bg-zinc-800 dark:text-zinc-300
                       focus:outline-none focus:ring-2 focus:ring-zinc-900/10 dark:focus:ring-zinc-100/10
                       transition-shadow"
          >
            <option value="all">All Actions</option>
            {actions.map((a) => (
              <option key={a} value={a.toLowerCase()}>
                {a}
              </option>
            ))}
          </select>

          <Button variant="outline" size="sm" onClick={load}>
            <RefreshCw className="mr-1.5 h-3.5 w-3.5" />
            Refresh
          </Button>
        </div>
      </motion.div>

      {/* ─── Table ─── */}
      <motion.div
        {...fadeUp}
        className="rounded-xl border border-zinc-200 dark:border-zinc-800 overflow-hidden bg-white dark:bg-zinc-900/40 shadow-sm"
      >
        <Table>
          <TableHeader>
            <TableRow className="border-b border-zinc-100 dark:border-zinc-800 bg-zinc-50/60 dark:bg-zinc-800/30">
              <TableHead className="w-[140px] text-[10px] font-semibold uppercase tracking-wider">
                Time
              </TableHead>
              <TableHead className="w-[100px] text-[10px] font-semibold uppercase tracking-wider">
                User
              </TableHead>
              <TableHead className="w-[120px] text-[10px] font-semibold uppercase tracking-wider">
                Action
              </TableHead>
              <TableHead className="text-[10px] font-semibold uppercase tracking-wider">
                Details
              </TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            {filtered.length === 0 ? (
              <TableRow>
                <TableCell colSpan={4} className="h-32 text-center">
                  <p className="text-sm text-zinc-500">
                    {loading ? "Loading..." : "No audit logs"}
                  </p>
                </TableCell>
              </TableRow>
            ) : (
              filtered.map((l) => <AuditRow key={l.id} log={l} />)
            )}
          </TableBody>
        </Table>
      </motion.div>
    </motion.div>
  );
}
