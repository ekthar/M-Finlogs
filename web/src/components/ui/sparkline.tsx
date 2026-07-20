"use client";

import { cn } from "@/lib/utils";

interface SparklineProps {
  data: number[];
  color?: string;
  className?: string;
  height?: number;
  width?: number;
}

/**
 * Tiny SVG sparkline chart for metric cards.
 * Pure SVG — no dependencies, renders instantly.
 */
export function Sparkline({ data, color = "#10b981", className, height = 24, width = 64 }: SparklineProps) {
  if (!data || data.length < 2) return null;

  const max = Math.max(...data, 1);
  const min = Math.min(...data, 0);
  const range = max - min || 1;

  const points = data.map((value, i) => {
    const x = (i / (data.length - 1)) * width;
    const y = height - ((value - min) / range) * (height - 4) - 2;
    return `${x},${y}`;
  });

  const pathD = `M ${points.join(" L ")}`;
  // Fill area
  const fillD = `${pathD} L ${width},${height} L 0,${height} Z`;

  return (
    <svg
      viewBox={`0 0 ${width} ${height}`}
      className={cn("overflow-visible", className)}
      width={width}
      height={height}
    >
      {/* Gradient fill */}
      <defs>
        <linearGradient id={`spark-${color.replace("#", "")}`} x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%" stopColor={color} stopOpacity={0.2} />
          <stop offset="100%" stopColor={color} stopOpacity={0} />
        </linearGradient>
      </defs>
      <path d={fillD} fill={`url(#spark-${color.replace("#", "")})`} />
      <path d={pathD} fill="none" stroke={color} strokeWidth={1.5} strokeLinecap="round" strokeLinejoin="round" />
      {/* Last point dot */}
      <circle
        cx={(data.length - 1) / (data.length - 1) * width}
        cy={height - ((data[data.length - 1] - min) / range) * (height - 4) - 2}
        r={2}
        fill={color}
      />
    </svg>
  );
}
