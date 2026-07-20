import { cn } from "@/lib/utils";

function Skeleton({ className, ...props }: React.HTMLAttributes<HTMLDivElement>) {
  return (
    <div
      className={cn(
        "animate-pulse rounded-xl bg-zinc-100 dark:bg-zinc-800",
        className
      )}
      {...props}
    />
  );
}

function TableSkeleton({ rows = 5, cols = 6 }: { rows?: number; cols?: number }) {
  return (
    <div className="rounded-xl border border-zinc-200/60 overflow-hidden dark:border-zinc-700/60">
      <div className="bg-zinc-50/80 dark:bg-zinc-800/80 px-4 py-3 flex gap-4">
        {Array.from({ length: cols }).map((_, i) => (
          <Skeleton key={i} className="h-4 flex-1" />
        ))}
      </div>
      {Array.from({ length: rows }).map((_, r) => (
        <div key={r} className="flex gap-4 px-4 py-3 border-t border-zinc-100 dark:border-zinc-800">
          {Array.from({ length: cols }).map((_, c) => (
            <Skeleton key={c} className="h-4 flex-1" />
          ))}
        </div>
      ))}
    </div>
  );
}

function CardSkeleton() {
  return (
    <div className="rounded-2xl border border-zinc-200/60 bg-white/70 p-5 dark:border-zinc-700/60 dark:bg-zinc-900/70">
      <Skeleton className="h-3 w-20 mb-3" />
      <Skeleton className="h-7 w-32" />
    </div>
  );
}

function DashboardSkeleton() {
  return (
    <div className="space-y-6">
      <div><Skeleton className="h-7 w-40 mb-2" /><Skeleton className="h-4 w-60" /></div>
      <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-4 xl:grid-cols-5">
        {Array.from({ length: 5 }).map((_, i) => <CardSkeleton key={i} />)}
      </div>
      <Skeleton className="h-48 w-full rounded-2xl" />
    </div>
  );
}

export { Skeleton, TableSkeleton, CardSkeleton, DashboardSkeleton };
