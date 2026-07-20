"use client";

import { createContext, useContext, useState, useEffect, type ReactNode } from "react";
import { getCurrentFinancialYear } from "@/lib/financial-year";

interface AppContextType {
  companyId: string;
  financialYear: string;
  username: string;
  role: string;
  setFinancialYear: (fy: string) => void;
  setCompanyId: (id: string) => void;
  setUser: (username: string, role: string) => void;
  apiParams: string;
}

const AppContext = createContext<AppContextType>({
  companyId: "cm_default_001",
  financialYear: getCurrentFinancialYear(),
  username: "admin",
  role: "admin",
  setFinancialYear: () => {},
  setCompanyId: () => {},
  setUser: () => {},
  apiParams: "",
});

export function AppProvider({ children }: { children: ReactNode }) {
  const [companyId, setCompanyId] = useState("cm_default_001");
  const [financialYear, setFinancialYear] = useState(getCurrentFinancialYear());
  const [username, setUsername] = useState("admin");
  const [role, setRole] = useState("admin");

  // Load from localStorage on mount
  useEffect(() => {
    const stored = localStorage.getItem("mfinlogs_context");
    if (stored) {
      try {
        const data = JSON.parse(stored);
        if (data.companyId) setCompanyId(data.companyId);
        if (data.financialYear) setFinancialYear(data.financialYear);
        if (data.username) setUsername(data.username);
        if (data.role) setRole(data.role);
      } catch { /* ignore */ }
    }
  }, []);

  // Persist to localStorage
  useEffect(() => {
    localStorage.setItem("mfinlogs_context", JSON.stringify({ companyId, financialYear, username, role }));
  }, [companyId, financialYear, username, role]);

  const apiParams = `companyId=${encodeURIComponent(companyId)}&financialYear=${encodeURIComponent(financialYear)}`;

  const setUser = (u: string, r: string) => { setUsername(u); setRole(r); };

  return (
    <AppContext.Provider value={{ companyId, financialYear, username, role, setFinancialYear, setCompanyId, setUser, apiParams }}>
      {children}
    </AppContext.Provider>
  );
}

export function useApp() {
  return useContext(AppContext);
}
