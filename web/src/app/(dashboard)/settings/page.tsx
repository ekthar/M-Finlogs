"use client";

import { motion } from "framer-motion";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Select } from "@/components/ui/select";
import { Badge } from "@/components/ui/badge";
import {
  Table, TableHeader, TableBody, TableRow, TableHead, TableCell,
} from "@/components/ui/data-table";
import { Info, Palette, Upload, Shield, Users, Key, DollarSign } from "lucide-react";

const fadeUp = {
  initial: { opacity: 0, y: 12 },
  animate: { opacity: 1, y: 0 },
  transition: { type: "spring" as const, bounce: 0, duration: 0.4 },
};

export default function SettingsPage() {
  return (
    <motion.div initial="initial" animate="animate" className="space-y-6 max-w-3xl">
      <motion.div {...fadeUp}>
        <h1 className="text-2xl font-semibold tracking-tight text-zinc-900 dark:text-zinc-100">Settings</h1>
        <p className="mt-0.5 text-sm text-zinc-500">Application preferences and administration</p>
      </motion.div>

      {/* About */}
      <motion.div {...fadeUp}>
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2"><Info className="h-5 w-5" /> About</CardTitle>
          </CardHeader>
          <CardContent>
            <div className="flex items-center gap-3">
              <div className="flex h-10 w-10 items-center justify-center rounded-xl bg-gradient-to-br from-zinc-900 to-zinc-700 dark:from-white dark:to-zinc-200">
                <span className="text-sm font-bold text-white dark:text-zinc-900">M</span>
              </div>
              <div>
                <p className="text-sm font-medium text-zinc-900 dark:text-zinc-100">M-Finlogs Web</p>
                <p className="text-xs text-zinc-500">
                  Developed by <span className="font-semibold text-zinc-700 dark:text-zinc-300">EKTHAR</span>
                </p>
              </div>
              <Badge className="ml-auto">v2.0.0-web</Badge>
            </div>
          </CardContent>
        </Card>
      </motion.div>

      {/* Preferences */}
      <motion.div {...fadeUp}>
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2"><Palette className="h-5 w-5" /> Preferences</CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <label className="flex items-center gap-2 cursor-pointer">
              <input type="checkbox" defaultChecked className="h-4 w-4 rounded border-zinc-300" />
              <span className="text-sm text-zinc-700 dark:text-zinc-300">Restrict Party Selection to List</span>
            </label>
            <div>
              <label className="mb-1.5 block text-xs font-medium text-zinc-600 dark:text-zinc-400">App Theme</label>
              <Select className="w-52">
                <option value="light">Light</option>
                <option value="dark">Dark</option>
                <option value="system">System</option>
              </Select>
            </div>
          </CardContent>
        </Card>
      </motion.div>

      {/* Data Import */}
      <motion.div {...fadeUp}>
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2"><Upload className="h-5 w-5" /> Data Import</CardTitle>
          </CardHeader>
          <CardContent>
            <div className="flex items-center gap-3">
              <Input type="file" accept=".csv,.xlsx" className="flex-1" />
              <Button variant="secondary" size="sm">Import</Button>
            </div>
          </CardContent>
        </Card>
      </motion.div>

      {/* Admin Panel */}
      <motion.div {...fadeUp} className="border-t-2 border-dashed border-zinc-200 pt-6 dark:border-zinc-700">
        <h2 className="text-lg font-semibold text-zinc-900 dark:text-zinc-100 flex items-center gap-2 mb-4">
          <Shield className="h-5 w-5 text-blue-600" /> Admin Panel
        </h2>

        {/* Opening Cash */}
        <Card className="mb-4">
          <CardHeader>
            <CardTitle className="flex items-center gap-2 text-base"><DollarSign className="h-4 w-4" /> Opening Cash</CardTitle>
          </CardHeader>
          <CardContent>
            <div className="flex items-center gap-3">
              <Input type="number" placeholder="Enter opening cash" className="flex-1" />
              <Button size="sm">Save</Button>
            </div>
            <p className="mt-2 text-xs text-zinc-400">This is used as the opening cash for the first day</p>
          </CardContent>
        </Card>

        {/* User Management */}
        <Card className="mb-4">
          <CardHeader>
            <CardTitle className="flex items-center gap-2 text-base"><Users className="h-4 w-4" /> User Management</CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="grid grid-cols-1 gap-3 sm:grid-cols-4 items-end">
              <Input placeholder="Username" />
              <Input type="password" placeholder="Password" />
              <Select>
                <option value="accounts">Accounts Role</option>
                <option value="admin">Admin Role</option>
              </Select>
              <Button>Add User</Button>
            </div>
            <Table>
              <TableHeader>
                <TableRow>
                  <TableHead>Username</TableHead>
                  <TableHead>Role</TableHead>
                  <TableHead className="w-24">Action</TableHead>
                </TableRow>
              </TableHeader>
              <TableBody>
                <TableRow>
                  <TableCell>admin</TableCell>
                  <TableCell><Badge>admin</Badge></TableCell>
                  <TableCell>—</TableCell>
                </TableRow>
              </TableBody>
            </Table>
          </CardContent>
        </Card>

        {/* Change Password */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2 text-base"><Key className="h-4 w-4" /> Change Password</CardTitle>
          </CardHeader>
          <CardContent>
            <div className="grid grid-cols-1 gap-3 sm:grid-cols-3 items-end">
              <Input placeholder="Username" />
              <Input type="password" placeholder="New Password" />
              <Button>Update</Button>
            </div>
          </CardContent>
        </Card>
      </motion.div>
    </motion.div>
  );
}
