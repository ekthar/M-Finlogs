#include "data/PasswordVerifier.h"

#include <QByteArray>
#include <QCryptographicHash>

extern "C" {
#include "argon2.h"
}

namespace mfinlogs::data {

QString sha256HexOf(const QString& value) {
    return QString::fromLatin1(
        QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool verifyPassword(const QString& storedHash, const QString& password) {
    const QString hash = storedHash.trimmed();
    if (hash.isEmpty()) {
        return false;
    }

    // Argon2 encoded hash (e.g. "$argon2id$v=19$m=65536,t=3,p=4$...").
    if (hash.startsWith(QStringLiteral("$argon2"))) {
        const QByteArray encoded = hash.toUtf8();
        const QByteArray pwd = password.toUtf8();
        int rc;
        if (hash.startsWith(QStringLiteral("$argon2id"))) {
            rc = argon2id_verify(encoded.constData(), pwd.constData(), static_cast<size_t>(pwd.size()));
        } else if (hash.startsWith(QStringLiteral("$argon2i"))) {
            rc = argon2i_verify(encoded.constData(), pwd.constData(), static_cast<size_t>(pwd.size()));
        } else if (hash.startsWith(QStringLiteral("$argon2d"))) {
            rc = argon2d_verify(encoded.constData(), pwd.constData(), static_cast<size_t>(pwd.size()));
        } else {
            return false;
        }
        return rc == ARGON2_OK;
    }

    // Otherwise treat as SHA-256 hex (constant-ish comparison is fine here).
    return hash == sha256HexOf(password);
}

} // namespace mfinlogs::data
