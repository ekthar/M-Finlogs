"use client";

import { useState } from "react";
import { motion } from "framer-motion";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Select } from "@/components/ui/select";
import { UserPlus, RefreshCw } from "lucide-react";

const fadeUp = {
  initial: { opacity: 0, y: 12 },
  animate: { opacity: 1, y: 0 },
  transition: { type: "spring" as const, bounce: 0, duration: 0.4 },
};

export default function PartiesPage() {
  const [creditAllowed, setCreditAllowed] = useState(false);

  return (
    <motion.div initial="initial" animate="animate" className="space-y-6 max-w-2xl">
      <motion.div {...fadeUp}>
        <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Parties</h1>
        <p className="mt-0.5 text-sm text-zinc-500">Create and manage parties (customers, suppliers, accounts)</p>
      </motion.div>

      {/* Create Party */}
      <motion.div {...fadeUp}>
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <UserPlus className="h-5 w-5" /> Add New Party
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div>
              <label className="mb-1.5 block text-xs font-medium text-zinc-600 dark:text-zinc-400">Party Name</label>
              <Input placeholder="Enter name..." />
            </div>
            <div>
              <label className="mb-1.5 block text-xs font-medium text-zinc-600 dark:text-zinc-400">Type</label>
              <Select>
                <option value="Customer">Customer</option>
                <option value="Credit Customer">Credit Customer</option>
                <option value="Supplier">Supplier</option>
                <option value="Expense Account">Expense Account</option>
                <option value="Bank">Bank Account</option>
              </Select>
            </div>
            <label className="flex items-center gap-2 cursor-pointer">
              <input
                type="checkbox"
                checked={creditAllowed}
                onChange={(e) => setCreditAllowed(e.target.checked)}
                className="h-4 w-4 rounded border-zinc-300 text-zinc-900 focus:ring-zinc-900"
              />
              <span className="text-sm text-zinc-700 dark:text-zinc-300">Allow Credit</span>
            </label>
            <Button className="w-full">Create Party</Button>
          </CardContent>
        </Card>
      </motion.div>

      {/* Rename Party */}
      <motion.div {...fadeUp}>
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <RefreshCw className="h-5 w-5" /> Rename Party (Admin)
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div>
              <label className="mb-1.5 block text-xs font-medium text-zinc-600 dark:text-zinc-400">Select Party</label>
              <Input placeholder="Search party to rename..." />
            </div>
            <div>
              <label className="mb-1.5 block text-xs font-medium text-zinc-600 dark:text-zinc-400">New Name</label>
              <Input placeholder="Enter new name..." />
            </div>
            <Button variant="secondary" className="w-full">Rename Party</Button>
          </CardContent>
        </Card>
      </motion.div>
    </motion.div>
  );
}
