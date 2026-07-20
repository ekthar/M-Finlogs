/**
 * Financial year utilities for Indian accounting (April – March)
 */

export function financialYearForDate(date: Date): string {
  const month = date.getMonth() + 1; // 1-based
  const year = date.getFullYear();
  const startYear = month >= 4 ? year : year - 1;
  return `${startYear}-${startYear + 1}`;
}

export function parseFinancialYear(fy: string): { start: Date; end: Date } {
  const match = fy.match(/^(\d{4})-(\d{4})$/);
  if (!match) throw new Error("Invalid financial year format. Use YYYY-YYYY");

  const startYear = parseInt(match[1]);
  const endYear = parseInt(match[2]);

  if (endYear !== startYear + 1) {
    throw new Error("Financial year must be consecutive years");
  }

  return {
    start: new Date(startYear, 3, 1), // April 1
    end: new Date(endYear, 2, 31), // March 31
  };
}

export function getCurrentFinancialYear(): string {
  return financialYearForDate(new Date());
}

export function getFinancialYearOptions(count: number = 5): string[] {
  const current = new Date();
  const startYear = current.getMonth() >= 3 ? current.getFullYear() : current.getFullYear() - 1;
  const years: string[] = [];
  for (let i = 0; i < count; i++) {
    const y = startYear - i;
    years.push(`${y}-${y + 1}`);
  }
  return years;
}
