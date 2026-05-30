#pragma once

#include <QString>

namespace mfinlogs::data {

// Centralized password verification supporting both formats found in M-Finlogs
// databases:
//   * SHA-256 hex (legacy native + old Electron default admin)
//   * Argon2id encoded strings ("$argon2id$...") from the Electron/Python
//     backend (argon2-cffi), verified with the vendored reference Argon2.
//
// Returns true if `password` matches `storedHash`.
bool verifyPassword(const QString& storedHash, const QString& password);

// SHA-256 hex of the UTF-8 bytes of `value` (the native storage format).
QString sha256HexOf(const QString& value);

} // namespace mfinlogs::data
