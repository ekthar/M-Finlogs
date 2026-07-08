from fastapi import FastAPI, HTTPException, Response, Depends, Header
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import pyodbc
import hashlib
import datetime
import time
import os
import re
import logging
import tempfile
import smtplib
import urllib.request
import uuid
import jwt
import threading
from collections import deque
from argon2 import PasswordHasher
from argon2.exceptions import VerifyMismatchError, InvalidHashError
from functools import wraps
from email.message import EmailMessage
from typing import Optional, List
from config import SQL_DATABASE, SQL_DRIVER, SQL_SERVER, SQL_USERNAME, SQL_PASSWORD
try:
    from config import BACKUP_TARGET_DIR
except Exception:
    BACKUP_TARGET_DIR = ""

CONFIG_DIR = os.environ.get("FINLOGS_CONFIG_DIR", os.getcwd())
CONFIG_FILE = os.path.join(CONFIG_DIR, "db_config.json")

def load_runtime_config():
    data = {}
    if os.path.exists(CONFIG_FILE):
        try:
            import json
            with open(CONFIG_FILE, "r") as f:
                data = json.load(f) or {}
        except Exception:
            data = {}

    auth_type = data.get("auth_type") or ("windows" if not SQL_USERNAME else "sql")
    username = data.get("username") if auth_type == "sql" else ""
    return {
        "server": data.get("server", SQL_SERVER),
        "database": data.get("database", SQL_DATABASE),
        "auth_type": auth_type,
        "username": username or SQL_USERNAME,
        "password": data.get("password", SQL_PASSWORD),
        "backup_dir": data.get("backup_dir") or BACKUP_TARGET_DIR
    }

def build_connection_string(database: str = None) -> str:
    cfg = load_runtime_config()
    db = database or cfg["database"]
    if cfg["auth_type"] == "sql" and cfg["username"]:
        return f"DRIVER={SQL_DRIVER};SERVER={cfg['server']};DATABASE={db};UID={cfg['username']};PWD={cfg['password']};TrustServerCertificate=yes;"
    return f"DRIVER={SQL_DRIVER};SERVER={cfg['server']};DATABASE={db};Trusted_Connection=yes;TrustServerCertificate=yes;"

def get_backup_target_dir() -> str:
    return load_runtime_config().get("backup_dir", "") or ""

def get_runtime_database_name() -> str:
    return load_runtime_config().get("database", SQL_DATABASE) or SQL_DATABASE

app = FastAPI()

# LAN-only trusted-network app: permissive CORS is intentional here (no public
# internet exposure), so an explicit origin allowlist would add complexity
# without a corresponding security benefit for this deployment model.
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# JWT Configuration
JWT_SECRET_KEY = os.environ.get("JWT_SECRET_KEY", "")
JWT_ALGORITHM = "HS256"
JWT_EXPIRATION_HOURS = 24
DBCONFIG_UNLOCK_MAX_ATTEMPTS = 5
DBCONFIG_UNLOCK_LOCK_MINUTES = 15
DBCONFIG_UNLOCK_PASSWORD_KEY = "db_config_unlock_password"
DBCONFIG_UNLOCK_FAIL_COUNT_KEY = "db_config_unlock_fail_count"
DBCONFIG_UNLOCK_LOCKED_UNTIL_KEY = "db_config_unlock_locked_until"

if not JWT_SECRET_KEY or len(JWT_SECRET_KEY.strip()) < 32:
    raise RuntimeError("JWT_SECRET_KEY must be set to a strong secret with at least 32 characters")

# Password hashing configuration (Argon2id)
password_hasher = PasswordHasher(
    time_cost=3,
    memory_cost=65536,
    parallelism=2,
    hash_len=32,
    salt_len=16,
)


def hash_user_password(plain_password: str) -> str:
    return password_hasher.hash(plain_password)


def is_legacy_sha256_hash(stored_hash: Optional[str]) -> bool:
    return bool(stored_hash and re.fullmatch(r"[a-f0-9]{64}", stored_hash))


def verify_user_password(stored_hash: Optional[str], plain_password: str) -> tuple[bool, bool]:
    """
    Returns: (is_valid, needs_upgrade)
    needs_upgrade is True for legacy SHA-256 hashes so caller can migrate.
    """
    if not stored_hash:
        return False, False

    if is_legacy_sha256_hash(stored_hash):
        legacy_hash = hashlib.sha256(plain_password.encode()).hexdigest()
        return legacy_hash == stored_hash, True

    try:
        return password_hasher.verify(stored_hash, plain_password), password_hasher.check_needs_rehash(stored_hash)
    except (VerifyMismatchError, InvalidHashError):
        return False, False

def create_access_token(username: str, role: str) -> str:
    """Create a JWT access token"""
    payload = {
        "username": username,
        "role": role,
        "exp": datetime.datetime.utcnow() + datetime.timedelta(hours=JWT_EXPIRATION_HOURS),
        "iat": datetime.datetime.utcnow()
    }
    token = jwt.encode(payload, JWT_SECRET_KEY, algorithm=JWT_ALGORITHM)
    return token

def verify_token(token: str) -> dict:
    """Verify JWT token and return payload"""
    try:
        payload = jwt.decode(token, JWT_SECRET_KEY, algorithms=[JWT_ALGORITHM])
        return payload
    except jwt.ExpiredSignatureError:
        raise HTTPException(status_code=401, detail="Token expired")
    except jwt.InvalidTokenError:
        raise HTTPException(status_code=401, detail="Invalid token")

def verify_auth_header(authorization: Optional[str] = Header(None)) -> dict:
    """Extract and verify token from Authorization header"""
    if not authorization:
        raise HTTPException(status_code=401, detail="Missing authorization header")
    
    parts = authorization.split()
    if len(parts) != 2 or parts[0].lower() != "bearer":
        raise HTTPException(status_code=401, detail="Invalid authorization header format")
    
    token = parts[1]
    return verify_token(token)

def require_admin(payload: dict):
    """Verify user has admin role"""
    if payload.get("role") != "admin":
        raise HTTPException(status_code=403, detail="Admin role required")
    return payload

def require_authenticated(authorization: Optional[str] = Header(None)) -> dict:
    return verify_auth_header(authorization)

logger = logging.getLogger("mfinlogs")
if not logger.handlers:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")

def log_exception(context: str, err: Exception):
    try:
        logger.exception("%s: %s", context, err)
    except Exception:
        pass


def close_conn_safely(conn):
    try:
        if conn:
            conn.close()
    except Exception as err:
        log_exception("close_conn_safely", err)


def safe_error_response(context: str, err: Exception, message: str = "Operation failed"):
    log_exception(context, err)
    return {"status": "Error", "detail": message}

REPORT_CACHE_TTL_SEC = 300
REPORT_CACHE_MAX_SIZE = 200
report_cache = {}

def cache_get(key):
    entry = report_cache.get(key)
    if not entry:
        return None
    ts, data = entry
    if time.time() - ts > REPORT_CACHE_TTL_SEC:
        report_cache.pop(key, None)
        return None
    return data

def cache_set(key, data):
    if len(report_cache) > REPORT_CACHE_MAX_SIZE:
        # Evict oldest entries (by timestamp) until back under the cap.
        oldest_first = sorted(report_cache.items(), key=lambda item: item[1][0])
        num_to_evict = len(report_cache) - REPORT_CACHE_MAX_SIZE
        for evict_key, _ in oldest_first[:num_to_evict]:
            report_cache.pop(evict_key, None)
    report_cache[key] = (time.time(), data)

def invalidate_report_cache():
    report_cache.clear()

def parse_date_str(value: Optional[str]) -> Optional[datetime.date]:
    if not value:
        return None
    try:
        return datetime.datetime.strptime(value, "%Y-%m-%d").date()
    except Exception:
        return None

def resolve_date_range(start: Optional[str], end: Optional[str], days: int = 30):
    end_date = parse_date_str(end) or datetime.date.today()
    start_date = parse_date_str(start) or (end_date - datetime.timedelta(days=max(days, 1) - 1))
    if start_date > end_date:
        start_date, end_date = end_date, start_date
    return start_date, end_date

def financial_year_for_date(dt: datetime.date) -> str:
    start_year = dt.year if dt.month >= 4 else dt.year - 1
    return f"{start_year}-{start_year + 1}"

def parse_financial_year(financial_year: str):
    m = re.match(r"^(\d{4})-(\d{4})$", (financial_year or "").strip())
    if not m:
        raise ValueError("Invalid financial year format. Use YYYY-YYYY")
    start_year = int(m.group(1))
    end_year = int(m.group(2))
    if end_year != start_year + 1:
        raise ValueError("Financial year must be consecutive years")
    return datetime.date(start_year, 4, 1), datetime.date(end_year, 3, 31)

def set_text_setting(key: str, value: str):
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("SELECT COUNT(*) FROM app_settings WHERE setting_key=?", (key,))
    exists = cursor.fetchone()[0] > 0
    if exists:
        cursor.execute("UPDATE app_settings SET setting_value=? WHERE setting_key=?", (value, key))
    else:
        cursor.execute("INSERT INTO app_settings (setting_key, setting_value) VALUES (?, ?)", (key, value))
    conn.close()

def get_selected_financial_year(cursor=None) -> str:
    global current_financial_year
    if current_financial_year:
        return current_financial_year

    close_conn = False
    conn = None
    try:
        if cursor is None:
            conn = get_db_connection()
            cursor = conn.cursor()
            close_conn = True

        cursor.execute("SELECT setting_value FROM app_settings WHERE setting_key='active_financial_year'")
        row = cursor.fetchone()
        if row and row[0]:
            current_financial_year = str(row[0]).strip()
        else:
            current_financial_year = financial_year_for_date(datetime.date.today())
            cursor.execute(
                "IF EXISTS (SELECT 1 FROM app_settings WHERE setting_key=?) "
                "UPDATE app_settings SET setting_value=? WHERE setting_key=? "
                "ELSE INSERT INTO app_settings (setting_key, setting_value) VALUES (?, ?)",
                ('active_financial_year', current_financial_year, 'active_financial_year', 'active_financial_year', current_financial_year)
            )
    except Exception:
        current_financial_year = financial_year_for_date(datetime.date.today())
    finally:
        if close_conn and conn:
            conn.close()

    return current_financial_year

def get_financial_year_bounds(cursor=None):
    selected_fy = get_selected_financial_year(cursor)
    fy_start, fy_end = parse_financial_year(selected_fy)
    return selected_fy, fy_start, fy_end

def ensure_financial_year_column(cursor):
    cursor.execute("SELECT COUNT(*) FROM sys.tables WHERE name = 'transactions'")
    has_transactions = cursor.fetchone()[0] > 0
    if not has_transactions:
        return

    cursor.execute("SELECT COUNT(*) FROM sys.columns WHERE object_id = OBJECT_ID('transactions') AND name = 'financial_year'")
    fy_col_exists = cursor.fetchone()[0] > 0
    if not fy_col_exists:
        cursor.execute("ALTER TABLE transactions ADD financial_year NVARCHAR(9) NULL")

    cursor.execute("""
        UPDATE transactions
        SET financial_year = CASE
            WHEN MONTH(txn_date) >= 4
                THEN CONCAT(YEAR(txn_date), '-', YEAR(txn_date) + 1)
            ELSE CONCAT(YEAR(txn_date) - 1, '-', YEAR(txn_date))
        END
        WHERE (financial_year IS NULL OR LTRIM(RTRIM(financial_year)) = '')
          AND txn_date IS NOT NULL
    """)

current_company = None
current_financial_year = None
initialized_dbs = set()  # Cache to track initialized databases

@app.on_event("startup")
def startup_init_db():
    try:
        init_db(SQL_DATABASE)
    except Exception:
        pass

@app.on_event("shutdown")
def shutdown_close_db_pool():
    try:
        _conn_pool.close_all()
    except Exception:
        pass

def normalize_company(name: str) -> str:
    base = re.sub(r"\s+", "_", name.strip().lower())
    return re.sub(r"[^a-z0-9_]+", "", base) or "default"

def normalize_party_key(name: str) -> str:
    cleaned = re.sub(r"\s+", " ", str(name or "").strip())
    return cleaned.lower().replace(" ", "_")

def init_db(company_name: str):
    # Skip if already initialized in this session
    db_key = normalize_company(company_name)
    if db_key in initialized_dbs:
        return
    
    conn = pyodbc.connect(build_connection_string(), autocommit=True)
    cursor = conn.cursor()

    # Create tables using SQL Server syntax
    cursor.execute("""
        IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'users')
        CREATE TABLE users (
            username NVARCHAR(255) PRIMARY KEY,
            password_hash NVARCHAR(255),
            role NVARCHAR(50)
        )
    """)
    cursor.execute("""
        IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'audit_logs')
        CREATE TABLE audit_logs (
            id INT PRIMARY KEY IDENTITY(1,1),
            timestamp DATETIME DEFAULT GETDATE(),
            username NVARCHAR(255),
            action NVARCHAR(255),
            details NVARCHAR(MAX),
            company NVARCHAR(255)
        )
    """)
    cursor.execute("""
        IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'app_settings')
        CREATE TABLE app_settings (
            setting_key NVARCHAR(255) PRIMARY KEY,
            setting_value NVARCHAR(MAX)
        )
    """)
    cursor.execute("""
        IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'daily_cash')
        CREATE TABLE daily_cash (
            cash_date DATE PRIMARY KEY,
            cash_in_hand DECIMAL(18,2),
            updated_at DATETIME DEFAULT GETDATE()
        )
    """)
    cursor.execute("""
        IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'parties')
        CREATE TABLE parties (
            party_id INT PRIMARY KEY IDENTITY(1,1),
            name NVARCHAR(255),
            normalized_name NVARCHAR(255) UNIQUE,
            type NVARCHAR(100),
            credit_allowed BIT
        )
    """)
    cursor.execute("""
        IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'transactions')
        CREATE TABLE transactions (
            txn_id INT PRIMARY KEY IDENTITY(1,1),
            txn_date DATE,
            bill_no NVARCHAR(255),
            party_id INT,
            txn_type NVARCHAR(50),
            payment_mode NVARCHAR(50),
            financial_year NVARCHAR(9),
            amount DECIMAL(18,2),
            FOREIGN KEY (party_id) REFERENCES parties (party_id)
        )
    """)

    # Migration: Add financial_year column if missing
    cursor.execute("SELECT COUNT(*) FROM sys.columns WHERE object_id = OBJECT_ID('transactions') AND name = 'financial_year'")
    fy_col_exists = cursor.fetchone()[0] > 0
    if not fy_col_exists:
        cursor.execute("ALTER TABLE transactions ADD financial_year NVARCHAR(9) NULL")

    # Backfill financial_year for historical rows
    cursor.execute("""
        UPDATE transactions
        SET financial_year = CASE
            WHEN MONTH(txn_date) >= 4
                THEN CONCAT(YEAR(txn_date), '-', YEAR(txn_date) + 1)
            ELSE CONCAT(YEAR(txn_date) - 1, '-', YEAR(txn_date))
        END
        WHERE financial_year IS NULL AND txn_date IS NOT NULL
    """)
    
    # Create indexes for faster queries
    cursor.execute("""
        IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_party_id' AND object_id = OBJECT_ID('transactions'))
        CREATE INDEX idx_transactions_party_id ON transactions(party_id)
    """)
    cursor.execute("""
        IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_date' AND object_id = OBJECT_ID('transactions'))
        CREATE INDEX idx_transactions_date ON transactions(txn_date)
    """)
    cursor.execute("""
        IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_type' AND object_id = OBJECT_ID('transactions'))
        CREATE INDEX idx_transactions_type ON transactions(txn_type)
    """)
    cursor.execute("""
        IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_mode' AND object_id = OBJECT_ID('transactions'))
        CREATE INDEX idx_transactions_mode ON transactions(payment_mode)
    """)
    cursor.execute("""
        IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_bill_no' AND object_id = OBJECT_ID('transactions'))
        CREATE INDEX idx_transactions_bill_no ON transactions(bill_no)
    """)
    cursor.execute("""
        IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_date_id' AND object_id = OBJECT_ID('transactions'))
        CREATE INDEX idx_transactions_date_id ON transactions(txn_date DESC, txn_id DESC)
    """)
    cursor.execute("""
        IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_date_type_mode' AND object_id = OBJECT_ID('transactions'))
        CREATE INDEX idx_transactions_date_type_mode ON transactions(txn_date, txn_type, payment_mode)
    """)
    cursor.execute("""
        IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_fy_date' AND object_id = OBJECT_ID('transactions'))
        CREATE INDEX idx_transactions_fy_date ON transactions(financial_year, txn_date)
    """)
    cursor.execute("""
        IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_parties_normalized' AND object_id = OBJECT_ID('parties'))
        CREATE INDEX idx_parties_normalized ON parties(normalized_name)
    """)

    # Initialize users table without default credentials
    # First-run setup will force admin password creation
    cursor.execute("SELECT COUNT(*) FROM users WHERE username='admin'")
    if cursor.fetchone()[0] == 0:
        # Create admin user WITHOUT password (password_hash = NULL means account disabled)
        cursor.execute("INSERT INTO users (username, password_hash, role) VALUES (?, ?, ?)", ('admin', None, 'admin'))
    
    # Don't create default 'user' account - ensure explicit user creation

    # Insert default company setting if not exists
    cursor.execute("IF NOT EXISTS (SELECT 1 FROM app_settings WHERE setting_key='company_name') INSERT INTO app_settings (setting_key, setting_value) VALUES (?, ?)", ("company_name", company_name))
    # Insert default active financial year if not exists
    cursor.execute(
        "IF NOT EXISTS (SELECT 1 FROM app_settings WHERE setting_key='active_financial_year') "
        "INSERT INTO app_settings (setting_key, setting_value) VALUES (?, ?)",
        ("active_financial_year", financial_year_for_date(datetime.date.today()))
    )

    # Migration: Add company column to audit_logs if it doesn't exist
    # Check if column exists first
    cursor.execute("SELECT COUNT(*) FROM sys.columns WHERE object_id = OBJECT_ID('audit_logs') AND name = 'company'")
    column_exists = cursor.fetchone()[0] > 0
    
    if not column_exists:
        # Add column with default value
        cursor.execute(f"ALTER TABLE audit_logs ADD company NVARCHAR(255) DEFAULT '{company_name}'")
        # Update existing NULL values
        cursor.execute("UPDATE audit_logs SET company = ? WHERE company IS NULL", (company_name,))

    conn.close()
    
    # Mark as initialized
    initialized_dbs.add(db_key)

def _configure_connection(conn):
    conn.setdecoding(pyodbc.SQL_CHAR, encoding='utf-8')
    conn.setdecoding(pyodbc.SQL_WCHAR, encoding='utf-8')
    conn.setencoding(encoding='utf-8')
    return conn


# --- Connection Pool ---
# Thread-safe pool of pyodbc connections keyed by connection string, to avoid
# opening a brand-new TCP/TDS connection to SQL Server on nearly every request.
class ConnectionPool:
    def __init__(self, max_size: int = 8):
        self._pool = deque()
        self._lock = threading.Lock()
        self._max_size = max_size

    def get(self, conn_string: str):
        with self._lock:
            while self._pool:
                conn = self._pool.popleft()
                try:
                    # Cheap probe to make sure the pooled connection is still alive.
                    conn.cursor().execute("SELECT 1")
                    return conn
                except Exception:
                    try:
                        conn.close()
                    except Exception:
                        pass
        # No healthy pooled connection available - create a fresh one, keeping
        # the existing retry-with-backoff behavior for transient DB errors.
        conn = _connect_with_retry(conn_string, timeout=5)
        return conn

    def release(self, conn):
        with self._lock:
            if len(self._pool) < self._max_size:
                self._pool.append(conn)
                return
        try:
            conn.close()
        except Exception:
            pass

    def close_all(self):
        with self._lock:
            while self._pool:
                conn = self._pool.popleft()
                try:
                    conn.close()
                except Exception:
                    pass


_conn_pool = ConnectionPool(max_size=8)


class _PooledConnectionProxy:
    """
    Thin wrapper around a raw pyodbc connection.

    pyodbc.Connection (like most C-extension DB driver objects) does not
    support arbitrary attribute assignment - `conn.close = <callable>` raises
    AttributeError at runtime. Every existing call site does `conn.close()`
    expecting that to fully close the connection; to make pooling transparent
    to those call sites without touching them, this proxy forwards everything
    to the real connection except close(), which releases it back to the pool
    instead of truly closing it.
    """
    __slots__ = ("_conn", "_released")

    def __init__(self, conn):
        object.__setattr__(self, "_conn", conn)
        object.__setattr__(self, "_released", False)

    def close(self):
        if self._released:
            return
        object.__setattr__(self, "_released", True)
        _conn_pool.release(self._conn)

    def __getattr__(self, name):
        return getattr(self._conn, name)

    def __setattr__(self, name, value):
        setattr(self._conn, name, value)


def get_db_connection(company: Optional[str] = None):
    comp = company or current_company or "default"
    # Don't call init_db on every connection - it's already initialized at startup.
    # Reuse a pooled connection when possible instead of opening a brand-new
    # connection on every call - this is a significant perf win under load.
    conn_string = build_connection_string()
    conn = _conn_pool.get(conn_string)
    return _PooledConnectionProxy(conn)

def get_master_connection():
    return _connect_with_retry(build_connection_string("master"), timeout=5)

def _is_transient_db_error(exc: Exception) -> bool:
    text = str(exc).lower()
    transient_markers = (
        "08001",  # client unable to establish connection
        "08s01",  # communication link failure
        "hyt00",  # timeout
        "timeout",
        "timed out",
        "login process due to delay",
        "communication link failure",
        "server connection"
    )
    return any(marker in text for marker in transient_markers)

def _connect_with_retry(conn_str: str, timeout: int = 5, attempts: int = 3):
    last_err = None
    for attempt in range(attempts):
        try:
            conn = pyodbc.connect(conn_str, autocommit=True, timeout=timeout)
            return _configure_connection(conn)
        except pyodbc.Error as exc:
            last_err = exc
            if attempt >= attempts - 1 or not _is_transient_db_error(exc):
                raise
            time.sleep(0.25 * (2 ** attempt))
    if last_err:
        raise last_err
    raise RuntimeError("Unable to establish database connection")

def get_or_create_party_id(cursor, party_name: str, party_type: str, credit_allowed: bool) -> int:
    normalized = normalize_party_key(party_name)
    if not normalized:
        raise ValueError("Party name is required")

    cursor.execute("SELECT party_id FROM parties WHERE normalized_name=?", (normalized,))
    row = cursor.fetchone()
    if row:
        return int(row[0])

    cursor.execute(
        """
        INSERT INTO parties (name, normalized_name, type, credit_allowed)
        OUTPUT INSERTED.party_id
        VALUES (?, ?, ?, ?)
        """,
        (str(party_name), normalized, party_type, 1 if credit_allowed else 0)
    )
    inserted = cursor.fetchone()
    if not inserted:
        raise RuntimeError(f"Could not create party: {party_name}")
    return int(inserted[0])

def get_default_backup_dir(cursor=None):
    configured = (get_backup_target_dir() or "").strip()
    if configured:
        return configured
    return "C:\\Finlogs"

def escape_sql_path(path: str) -> str:
    return path.replace("'", "''")

def escape_sql_identifier(identifier: str) -> str:
    return str(identifier or "").replace("]", "]]")

def get_desktop_company_backup_dir(company_name: str) -> str:
    desktop = os.path.join(os.path.expanduser("~"), "Desktop")
    folder = normalize_company(company_name) or "default"
    return os.path.join(desktop, folder)

# Remove Global conn
# conn = ... (Removed)

# init_db("default")  # Commented out - database should already exist

# Auth Models
class LoginRequest(BaseModel):
    username: str
    password: str

class SetupAdminRequest(BaseModel):
    password: str

class CreateUserRequest(BaseModel):
    username: str
    password: str
    role: str

class AuditLogRequest(BaseModel):
    username: str
    action: str
    details: str

class OpeningCashRequest(BaseModel):
    amount: float
    admin_user: str

class CashInHandRequest(BaseModel):
    date: str
    cash_in_hand: float
    admin_user: str

class CashInHandResetRequest(BaseModel):
    date: str
    admin_user: str

class ChangePasswordRequest(BaseModel):
    username: str
    new_password: str
    admin_user: str

class CompanyCreateRequest(BaseModel):
    name: str

class CompanySelectRequest(BaseModel):
    name: str

class FinancialYearSelectRequest(BaseModel):
    year: str

class InventoryProductRow(BaseModel):
    id: Optional[str] = None
    name: str
    qty: List[float] = []
    purchase_qty: Optional[List[float]] = []
    min_stock: Optional[float] = 0.0
    cost: Optional[float] = 0.0

class InventorySnapshotRequest(BaseModel):
    financial_year: str
    month: int
    rows: List[InventoryProductRow]
    updated_at: Optional[str] = None

class InventoryMasterRequest(BaseModel):
    financial_year: str
    rows: List[InventoryProductRow]
    updated_at: Optional[str] = None

class InventoryPdfMailRequest(BaseModel):
    financial_year: Optional[str] = None
    month: str
    rows: List[InventoryProductRow]
    to_email: str
    cc_email: Optional[str] = None
    sender_email: str
    sender_password: str
    smtp_host: str = "smtp.gmail.com"
    smtp_port: int = 587
    subject: Optional[str] = None
    notes: Optional[str] = None
    average_mode: str = "monthly"  # monthly | last7
    only_reorder: bool = False
    show_stock_value: bool = False

class InventoryPdfPreviewRequest(BaseModel):
    financial_year: Optional[str] = None
    month: str
    rows: List[InventoryProductRow]
    average_mode: str = "monthly"  # monthly | last7
    only_reorder: bool = False
    show_stock_value: bool = False

# Helper for Audit (Needs its own conn if called separately, but usually called within context? 
# Actually log_audit is a helper. Let's make it open its own conn to be safe and independent)
def log_audit(username, action, details, company=None):
    try:
        comp = company or current_company or "default"
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute("INSERT INTO audit_logs (username, action, details, company) VALUES (?, ?, ?, ?)", (username, action, details, comp))
        conn.close()
        invalidate_report_cache()
    except Exception as err:
        log_exception("log_audit", err)

def get_setting(key: str, default_val: float = 0.0) -> float:
    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute("SELECT setting_value FROM app_settings WHERE setting_key=?", (key,))
        row = cursor.fetchone()
        conn.close()
        if row and row[0] is not None:
            return float(row[0])
    except Exception as err:
        log_exception("get_setting", err)
    return float(default_val)

def _inventory_scope_key(company: Optional[str] = None) -> str:
    return normalize_company(company or current_company or "default")

def _inventory_snapshot_key(financial_year: str, month: int, company: Optional[str] = None) -> str:
    return f"inventory_snapshot:{_inventory_scope_key(company)}:{financial_year}:{int(month):02d}"

def _inventory_master_key(financial_year: str, company: Optional[str] = None) -> str:
    return f"inventory_master:{_inventory_scope_key(company)}:{financial_year}"

def _get_json_setting(cursor, key: str):
    cursor.execute("SELECT setting_value FROM app_settings WHERE setting_key=?", (key,))
    row = cursor.fetchone()
    if not row or row[0] in (None, ""):
        return None
    try:
        import json
        return json.loads(row[0])
    except Exception:
        return None

def _ensure_sql_index(cursor, index_name: str, table_name: str, create_sql: str):
    cursor.execute(
        "SELECT COUNT(*) "
        "FROM sys.indexes i "
        "INNER JOIN sys.tables t ON i.object_id=t.object_id "
        "WHERE i.name=? AND t.name=?",
        (index_name, table_name)
    )
    if cursor.fetchone()[0] > 0:
        return
    try:
        cursor.execute(create_sql)
    except pyodbc.ProgrammingError as e:
        message = str(e).lower()
        if "1913" in message or "already exists" in message:
            return
        raise

def _ensure_inventory_tables(cursor):
    # Create normalized inventory tables if they don't exist
    try:
        cursor.execute("SELECT COUNT(*) FROM sys.tables WHERE name='inventory_items'")
        exists = cursor.fetchone()[0] > 0
        if not exists:
            cursor.execute(
                "CREATE TABLE inventory_items ("
                "id INT IDENTITY(1,1) PRIMARY KEY,"
                "client_row_id NVARCHAR(128) NULL,"
                "company NVARCHAR(255) NOT NULL,"
                "name NVARCHAR(1024) NOT NULL,"
                "cost FLOAT NULL,"
                "min_stock FLOAT NULL,"
                "updated_at DATETIME2 NULL"
                ")"
            )
        else:
            cursor.execute("SELECT COUNT(*) FROM sys.columns WHERE object_id = OBJECT_ID('inventory_items') AND name = 'client_row_id'")
            has_client_id = cursor.fetchone()[0] > 0
            if not has_client_id:
                cursor.execute("ALTER TABLE inventory_items ADD client_row_id NVARCHAR(128) NULL")
        cursor.execute("SELECT COUNT(*) FROM sys.tables WHERE name='inventory_quantities'")
        exists_q = cursor.fetchone()[0] > 0
        if not exists_q:
            cursor.execute(
                "CREATE TABLE inventory_quantities ("
                "id INT IDENTITY(1,1) PRIMARY KEY,"
                "item_id INT NOT NULL,"
                "company NVARCHAR(255) NOT NULL,"
                "financial_year NVARCHAR(64) NOT NULL,"
                "month INT NOT NULL,"
                "day INT NOT NULL,"
                "qty FLOAT NOT NULL"
                ")"
            )
        cursor.execute("SELECT COUNT(*) FROM sys.tables WHERE name='inventory_purchases'")
        exists_p = cursor.fetchone()[0] > 0
        if not exists_p:
            cursor.execute(
                "CREATE TABLE inventory_purchases ("
                "id INT IDENTITY(1,1) PRIMARY KEY,"
                "item_id INT NOT NULL,"
                "company NVARCHAR(255) NOT NULL,"
                "financial_year NVARCHAR(64) NOT NULL,"
                "month INT NOT NULL,"
                "day INT NOT NULL,"
                "qty FLOAT NOT NULL"
                ")"
            )
        _ensure_sql_index(cursor, "idx_inventory_items_company_id", "inventory_items", "CREATE INDEX idx_inventory_items_company_id ON inventory_items (company, id)")
        _ensure_sql_index(cursor, "idx_inventory_items_company_client_row", "inventory_items", "CREATE INDEX idx_inventory_items_company_client_row ON inventory_items (company, client_row_id)")
        _ensure_sql_index(cursor, "idx_inventory_quantities_lookup", "inventory_quantities", "CREATE INDEX idx_inventory_quantities_lookup ON inventory_quantities (company, financial_year, month, item_id)")
        _ensure_sql_index(cursor, "idx_inventory_purchases_lookup", "inventory_purchases", "CREATE INDEX idx_inventory_purchases_lookup ON inventory_purchases (company, financial_year, month, item_id)")
        try:
            cursor.execute("UPDATE inventory_items SET client_row_id = CONCAT('inv-', CAST(id AS NVARCHAR(32))) WHERE client_row_id IS NULL OR LTRIM(RTRIM(client_row_id)) = ''")
        except Exception:
            pass
        try:
            _maybe_cleanup_inventory_duplicates(cursor)
        except Exception:
            pass
    except Exception as e:
        # Table creation can fail if running on a DB without permissions — log and continue
        log_exception("_ensure_inventory_tables", e)

def _maybe_cleanup_inventory_duplicates(cursor):
    # Remove legacy duplicate rows (same company + name) created by per-keystroke saves.
    try:
        company_scope = _inventory_scope_key()
        setting_key = f"inventory_dedup_done:{company_scope}"

        # Remove duplicate quantities first, then items, keeping the newest row per name.
        cursor.execute(
            "WITH d AS ("
            "SELECT id, ROW_NUMBER() OVER (PARTITION BY company, LOWER(LTRIM(RTRIM(name))) "
            "ORDER BY updated_at DESC, id DESC) AS rn "
            "FROM inventory_items WHERE company=?"
            ") "
            "DELETE q FROM inventory_quantities q INNER JOIN d ON q.item_id = d.id WHERE d.rn > 1",
            (company_scope,)
        )
        cursor.execute(
            "WITH d AS ("
            "SELECT id, ROW_NUMBER() OVER (PARTITION BY company, LOWER(LTRIM(RTRIM(name))) "
            "ORDER BY updated_at DESC, id DESC) AS rn "
            "FROM inventory_items WHERE company=?"
            ") "
            "DELETE p FROM inventory_purchases p INNER JOIN d ON p.item_id = d.id WHERE d.rn > 1",
            (company_scope,)
        )
        cursor.execute(
            "WITH d AS ("
            "SELECT id, ROW_NUMBER() OVER (PARTITION BY company, LOWER(LTRIM(RTRIM(name))) "
            "ORDER BY updated_at DESC, id DESC) AS rn "
            "FROM inventory_items WHERE company=?"
            ") "
            "DELETE FROM inventory_items WHERE id IN (SELECT id FROM d WHERE rn > 1)",
            (company_scope,)
        )

        cursor.execute("SELECT COUNT(*) FROM app_settings WHERE setting_key=?", (setting_key,))
        exists = cursor.fetchone()[0] > 0
        if exists:
            cursor.execute("UPDATE app_settings SET setting_value=? WHERE setting_key=?", ('1', setting_key))
        else:
            cursor.execute("INSERT INTO app_settings (setting_key, setting_value) VALUES (?, ?)", (setting_key, '1'))
    except Exception as e:
        log_exception("_maybe_cleanup_inventory_duplicates", e)

def _cleanup_inventory_draft_prefixes(cursor):
    try:
        company_scope = _inventory_scope_key()
        cursor.execute(
            "WITH draft AS ("
            "SELECT i.id "
            "FROM inventory_items i "
            "WHERE i.company=? "
            "AND LEN(LTRIM(RTRIM(i.name))) > 0 "
            "AND ISNULL(i.cost, 0) = 0 "
            "AND ISNULL(i.min_stock, 0) = 0 "
            "AND NOT EXISTS (SELECT 1 FROM inventory_quantities q WHERE q.item_id=i.id AND ISNULL(q.qty, 0) <> 0) "
            "AND NOT EXISTS (SELECT 1 FROM inventory_purchases p WHERE p.item_id=i.id AND ISNULL(p.qty, 0) <> 0) "
            "AND EXISTS ("
            "SELECT 1 FROM inventory_items full_item "
            "WHERE full_item.company=i.company "
            "AND full_item.id<>i.id "
            "AND LOWER(LTRIM(RTRIM(full_item.name))) LIKE LOWER(LTRIM(RTRIM(i.name))) + '%' "
            "AND LEN(LTRIM(RTRIM(full_item.name))) > LEN(LTRIM(RTRIM(i.name)))"
            ")"
            ") "
            "DELETE q FROM inventory_quantities q INNER JOIN draft d ON q.item_id=d.id",
            (company_scope,)
        )
        cursor.execute(
            "WITH draft AS ("
            "SELECT i.id "
            "FROM inventory_items i "
            "WHERE i.company=? "
            "AND LEN(LTRIM(RTRIM(i.name))) > 0 "
            "AND ISNULL(i.cost, 0) = 0 "
            "AND ISNULL(i.min_stock, 0) = 0 "
            "AND NOT EXISTS (SELECT 1 FROM inventory_quantities q WHERE q.item_id=i.id AND ISNULL(q.qty, 0) <> 0) "
            "AND NOT EXISTS (SELECT 1 FROM inventory_purchases p WHERE p.item_id=i.id AND ISNULL(p.qty, 0) <> 0) "
            "AND EXISTS ("
            "SELECT 1 FROM inventory_items full_item "
            "WHERE full_item.company=i.company "
            "AND full_item.id<>i.id "
            "AND LOWER(LTRIM(RTRIM(full_item.name))) LIKE LOWER(LTRIM(RTRIM(i.name))) + '%' "
            "AND LEN(LTRIM(RTRIM(full_item.name))) > LEN(LTRIM(RTRIM(i.name)))"
            ")"
            ") "
            "DELETE p FROM inventory_purchases p INNER JOIN draft d ON p.item_id=d.id",
            (company_scope,)
        )
        cursor.execute(
            "DELETE i FROM inventory_items i "
            "WHERE i.company=? "
            "AND LEN(LTRIM(RTRIM(i.name))) > 0 "
            "AND ISNULL(i.cost, 0) = 0 "
            "AND ISNULL(i.min_stock, 0) = 0 "
            "AND NOT EXISTS (SELECT 1 FROM inventory_quantities q WHERE q.item_id=i.id AND ISNULL(q.qty, 0) <> 0) "
            "AND NOT EXISTS (SELECT 1 FROM inventory_purchases p WHERE p.item_id=i.id AND ISNULL(p.qty, 0) <> 0) "
            "AND EXISTS ("
            "SELECT 1 FROM inventory_items full_item "
            "WHERE full_item.company=i.company "
            "AND full_item.id<>i.id "
            "AND LOWER(LTRIM(RTRIM(full_item.name))) LIKE LOWER(LTRIM(RTRIM(i.name))) + '%' "
            "AND LEN(LTRIM(RTRIM(full_item.name))) > LEN(LTRIM(RTRIM(i.name)))"
            ")",
            (company_scope,)
        )
    except Exception as e:
        log_exception("_cleanup_inventory_draft_prefixes", e)

def _migrate_inventory_from_app_settings(cursor, company_scope: str):
    # Migrate old JSON snapshots/master from app_settings into normalized tables if found
    try:
        # Enumerate keys for this company
        prefix_snapshot = f"inventory_snapshot:{company_scope}:"
        cursor.execute("SELECT setting_key, setting_value FROM app_settings WHERE setting_key LIKE ?", (prefix_snapshot + '%',))
        rows = cursor.fetchall()
        if rows:
            _ensure_inventory_tables(cursor)
            import json
            for key, val in rows:
                try:
                    data = json.loads(val) if val else None
                    if not data:
                        continue
                    fy = data.get('financial_year') or key.split(':')[-2]
                    month = int(data.get('month') or key.split(':')[-1])
                    for r in data.get('rows') or []:
                        name = (r.get('name') or '').strip()
                        if not name:
                            continue
                        cost = float(r.get('cost') or 0)
                        min_stock = float(r.get('min_stock') or 0)
                        # upsert item
                        cursor.execute("SELECT id FROM inventory_items WHERE company=? AND LOWER(name)=LOWER(?)", (company_scope, name))
                        found = cursor.fetchone()
                        if found:
                            item_id = found[0]
                            cursor.execute("UPDATE inventory_items SET client_row_id=COALESCE(client_row_id, CONCAT('inv-', CAST(id AS NVARCHAR(32)))), cost=?, min_stock=?, updated_at=? WHERE id=?", (cost, min_stock, datetime.datetime.utcnow(), item_id))
                        else:
                            cursor.execute("INSERT INTO inventory_items (client_row_id, company, name, cost, min_stock, updated_at) VALUES (?, ?, ?, ?, ?, ?)", (f"inv-{uuid.uuid4().hex}", company_scope, name, cost, min_stock, datetime.datetime.utcnow()))
                            cursor.execute("SELECT SCOPE_IDENTITY()")
                            item_id = int(cursor.fetchone()[0])
                        qtys = r.get('qty') or []
                        # delete existing qtys for that fy/month
                        cursor.execute("DELETE FROM inventory_quantities WHERE item_id=? AND company=? AND financial_year=? AND month=?", (item_id, company_scope, fy, month))
                        for i, q in enumerate(qtys):
                            try:
                                qv = float(q or 0)
                            except Exception:
                                qv = 0.0
                            cursor.execute("INSERT INTO inventory_quantities (item_id, company, financial_year, month, day, qty) VALUES (?, ?, ?, ?, ?, ?)", (item_id, company_scope, fy, month, i + 1, qv))
                except Exception as e:
                    log_exception("_migrate_inventory_from_app_settings_row", e)
            try:
                # remove migrated keys to avoid duplicate migration
                cursor.execute("DELETE FROM app_settings WHERE setting_key LIKE ?", (prefix_snapshot + '%',))
            except Exception:
                pass
    except Exception as e:
        log_exception("_migrate_inventory_from_app_settings", e)

def _get_inventory_rows_from_tables(cursor, company_scope: str, financial_year: str, month: int):
    # Ensure tables exist
    _ensure_inventory_tables(cursor)
    # Read items
    cursor.execute("SELECT id, client_row_id, name, cost, min_stock, updated_at FROM inventory_items WHERE company=? ORDER BY id", (company_scope,))
    items = cursor.fetchall()
    if not items:
        return []

    item_ids = [int(item[0]) for item in items]

    def fetch_day_values(table_name: str):
        values = {}
        chunk_size = 900
        for offset in range(0, len(item_ids), chunk_size):
            chunk = item_ids[offset:offset + chunk_size]
            placeholders = ",".join("?" for _ in chunk)
            cursor.execute(
                f"SELECT item_id, day, qty FROM {table_name} "
                f"WHERE company=? AND financial_year=? AND month=? AND item_id IN ({placeholders})",
                (company_scope, financial_year, int(month), *chunk)
            )
            for item_id, day, qty in cursor.fetchall():
                values.setdefault(int(item_id), {})[int(day)] = float(qty or 0)
        return values

    quantity_map = fetch_day_values("inventory_quantities")
    purchase_map = fetch_day_values("inventory_purchases")

    result = []
    for item in items:
        item_id, client_row_id, name, cost, min_stock, updated_at = item
        # Build qty array; default up to 31 days
        qtys = [0] * 31
        purchases = [0] * 31
        for d, qv in quantity_map.get(int(item_id), {}).items():
            if 1 <= d <= 31:
                qtys[d - 1] = qv
        for d, qv in purchase_map.get(int(item_id), {}).items():
            if 1 <= d <= 31:
                purchases[d - 1] = qv
        result.append({
            'id': client_row_id or f'inv-{item_id}',
            'name': name,
            'cost': float(cost or 0),
            'min_stock': float(min_stock or 0),
            'qty': qtys,
            'purchase_qty': purchases,
            'updated_at': updated_at.isoformat() if isinstance(updated_at, datetime.datetime) else (str(updated_at) if updated_at else None)
        })
    return result

def _save_inventory_rows_to_tables(cursor, company_scope: str, financial_year: str, month: int, rows):
    _ensure_inventory_tables(cursor)
    # Upsert each item and write quantities (delete existing quantities for item/fy/month)
    for r in (rows or []):
        row_id = str(r.get('id') or '').strip()
        name = (r.get('name') or '').strip()
        if not name:
            continue
        cost = float(r.get('cost') or 0)
        min_stock = float(r.get('min_stock') or 0)
        if not row_id:
            row_id = f"inv-{uuid.uuid4().hex}"
        # find existing item by stable row id first, then legacy name fallback
        cursor.execute("SELECT id FROM inventory_items WHERE company=? AND client_row_id=?", (company_scope, row_id))
        found = cursor.fetchone()
        if not found:
            cursor.execute("SELECT id FROM inventory_items WHERE company=? AND LOWER(name)=LOWER(?)", (company_scope, name))
            found = cursor.fetchone()
        if found:
            item_id = found[0]
            cursor.execute("UPDATE inventory_items SET client_row_id=?, name=?, cost=?, min_stock=?, updated_at=? WHERE id=?", (row_id, name, cost, min_stock, datetime.datetime.utcnow(), item_id))
        else:
            cursor.execute("INSERT INTO inventory_items (client_row_id, company, name, cost, min_stock, updated_at) VALUES (?, ?, ?, ?, ?, ?)", (row_id, company_scope, name, cost, min_stock, datetime.datetime.utcnow()))
            cursor.execute("SELECT SCOPE_IDENTITY()")
            item_id = int(cursor.fetchone()[0])
        # delete existing quantities for this item for given fy/month
        cursor.execute("DELETE FROM inventory_quantities WHERE item_id=? AND company=? AND financial_year=? AND month=?", (item_id, company_scope, financial_year, int(month)))
        cursor.execute("DELETE FROM inventory_purchases WHERE item_id=? AND company=? AND financial_year=? AND month=?", (item_id, company_scope, financial_year, int(month)))
        qtys = r.get('qty') or []
        purchases = r.get('purchase_qty') or []
        for i, q in enumerate(qtys):
            try:
                qv = float(q or 0)
            except Exception:
                qv = 0.0
            cursor.execute("INSERT INTO inventory_quantities (item_id, company, financial_year, month, day, qty) VALUES (?, ?, ?, ?, ?, ?)", (item_id, company_scope, financial_year, int(month), int(i + 1), qv))
        for i, q in enumerate(purchases):
            try:
                qv = float(q or 0)
            except Exception:
                qv = 0.0
            if qv <= 0:
                continue
            cursor.execute("INSERT INTO inventory_purchases (item_id, company, financial_year, month, day, qty) VALUES (?, ?, ?, ?, ?, ?)", (item_id, company_scope, financial_year, int(month), int(i + 1), qv))

def _set_json_setting(cursor, key: str, value):
    import json
    payload = json.dumps(value, ensure_ascii=False)
    cursor.execute("SELECT COUNT(*) FROM app_settings WHERE setting_key=?", (key,))
    exists = cursor.fetchone()[0] > 0
    if exists:
        cursor.execute("UPDATE app_settings SET setting_value=? WHERE setting_key=?", (payload, key))
    else:
        cursor.execute("INSERT INTO app_settings (setting_key, setting_value) VALUES (?, ?)", (key, payload))

def _delete_legacy_inventory_settings(cursor, company_scope: str):
    cursor.execute("DELETE FROM app_settings WHERE setting_key LIKE ?", (f"inventory_snapshot:{company_scope}:%",))
    cursor.execute("DELETE FROM app_settings WHERE setting_key LIKE ?", (f"inventory_master:{company_scope}:%",))

@app.get("/inventory/snapshot")
def get_inventory_snapshot(financial_year: Optional[str] = None, month: Optional[int] = None):
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        fy = (financial_year or get_selected_financial_year(cursor) or "").strip()
        month_num = int(month) if month is not None else datetime.date.today().month
        if month_num < 1 or month_num > 12:
            raise HTTPException(status_code=400, detail="Invalid month")
        company_scope = _inventory_scope_key()
        # Try normalized tables first
        try:
            rows = _get_inventory_rows_from_tables(cursor, company_scope, fy, month_num)
            if rows:
                conn.close()
                return {"found": True, "financial_year": fy, "month": month_num, "rows": rows, "updated_at": None}
        except Exception as e:
            log_exception("get_inventory_snapshot_tables", e)

        _delete_legacy_inventory_settings(cursor, company_scope)
        conn.commit()
        conn.close()
        return {"found": False, "financial_year": fy, "month": month_num, "rows": [], "updated_at": None}
    except HTTPException:
        close_conn_safely(conn)
        raise
    except Exception as e:
        close_conn_safely(conn)
        log_exception("get_inventory_snapshot", e)
        return {"found": False, "financial_year": financial_year, "month": month, "rows": [], "updated_at": None}

@app.post("/inventory/snapshot")
def save_inventory_snapshot(req: InventorySnapshotRequest, authorization: Optional[str] = Header(None)):
    require_authenticated(authorization)
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        fy = (req.financial_year or "").strip()
        month_num = int(req.month)
        if not fy:
            raise HTTPException(status_code=400, detail="Financial year is required")
        if month_num < 1 or month_num > 12:
            raise HTTPException(status_code=400, detail="Invalid month")
        company_scope = _inventory_scope_key()
        try:
            _save_inventory_rows_to_tables(cursor, company_scope, fy, month_num, [r.model_dump() if hasattr(r, "model_dump") else r.dict() for r in (req.rows or [])])
            conn.commit()
            conn.close()
            return {"status": "Saved", "found": True}
        except Exception as e:
            raise RuntimeError(f"Failed to save inventory tables: {str(e)}")
    except HTTPException:
        close_conn_safely(conn)
        raise
    except Exception as e:
        close_conn_safely(conn)
        log_exception("save_inventory_snapshot", e)
        raise HTTPException(status_code=500, detail=f"Failed to save inventory snapshot: {str(e)}")

@app.get("/inventory/master")
def get_inventory_master(financial_year: Optional[str] = None):
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        fy = (financial_year or get_selected_financial_year(cursor) or "").strip()
        company_scope = _inventory_scope_key()
        try:
            _ensure_inventory_tables(cursor)
            cursor.execute("SELECT client_row_id, name, cost, min_stock, updated_at FROM inventory_items WHERE company=? ORDER BY id", (company_scope,))
            items = cursor.fetchall()
            if items:
                rows = []
                for it in items:
                    client_row_id, name, cost, min_stock, updated_at = it
                    rows.append({
                        'id': client_row_id,
                        'name': name,
                        'cost': float(cost or 0),
                        'min_stock': float(min_stock or 0),
                        'updated_at': updated_at.isoformat() if isinstance(updated_at, datetime.datetime) else (str(updated_at) if updated_at else None)
                    })
                conn.close()
                return {"found": True, "financial_year": fy, "rows": rows, "updated_at": None}
        except Exception as e:
            log_exception("get_inventory_master_tables", e)

        _delete_legacy_inventory_settings(cursor, company_scope)
        conn.commit()
        conn.close()
        return {"found": False, "financial_year": fy, "rows": [], "updated_at": None}
    except Exception as e:
        close_conn_safely(conn)
        log_exception("get_inventory_master", e)
        return {"found": False, "financial_year": financial_year, "rows": [], "updated_at": None}

@app.post("/inventory/master")
def save_inventory_master(req: InventoryMasterRequest, authorization: Optional[str] = Header(None)):
    require_authenticated(authorization)
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        fy = (req.financial_year or "").strip()
        if not fy:
            raise HTTPException(status_code=400, detail="Financial year is required")
        company_scope = _inventory_scope_key()
        try:
            # Persist into inventory_items table
            _ensure_inventory_tables(cursor)
            for r in (req.rows or []):
                row_id = str(r.get('id') if isinstance(r, dict) else (r.id if hasattr(r, 'id') else '')).strip()
                name = (r.get('name') if isinstance(r, dict) else (r.name if hasattr(r, 'name') else '') ) or ''
                name = str(name).strip()
                if not name:
                    continue
                cost = float(r.get('cost') if isinstance(r, dict) else (r.cost if hasattr(r, 'cost') else 0) or 0)
                min_stock = float(r.get('min_stock') if isinstance(r, dict) else (r.min_stock if hasattr(r, 'min_stock') else 0) or 0)
                if not row_id:
                    row_id = f"inv-{uuid.uuid4().hex}"
                cursor.execute("SELECT id FROM inventory_items WHERE company=? AND client_row_id=?", (company_scope, row_id))
                found = cursor.fetchone()
                if not found:
                    cursor.execute("SELECT id FROM inventory_items WHERE company=? AND LOWER(name)=LOWER(?)", (company_scope, name))
                    found = cursor.fetchone()
                if found:
                    cursor.execute("UPDATE inventory_items SET client_row_id=?, name=?, cost=?, min_stock=?, updated_at=? WHERE id=?", (row_id, name, cost, min_stock, datetime.datetime.utcnow(), found[0]))
                else:
                    cursor.execute("INSERT INTO inventory_items (client_row_id, company, name, cost, min_stock, updated_at) VALUES (?, ?, ?, ?, ?, ?)", (row_id, company_scope, name, cost, min_stock, datetime.datetime.utcnow()))
            keep_ids = []
            for r in (req.rows or []):
                row_id = str(r.get('id') if isinstance(r, dict) else (r.id if hasattr(r, 'id') else '')).strip()
                name = (r.get('name') if isinstance(r, dict) else (r.name if hasattr(r, 'name') else '') ) or ''
                name = str(name).strip()
                if row_id and name:
                    keep_ids.append(row_id)
            if keep_ids:
                placeholders = ",".join("?" for _ in keep_ids)
                cursor.execute(
                    f"DELETE q FROM inventory_quantities q INNER JOIN inventory_items i ON q.item_id=i.id WHERE i.company=? AND i.client_row_id IS NOT NULL AND i.client_row_id NOT IN ({placeholders})",
                    (company_scope, *keep_ids)
                )
                cursor.execute(
                    f"DELETE p FROM inventory_purchases p INNER JOIN inventory_items i ON p.item_id=i.id WHERE i.company=? AND i.client_row_id IS NOT NULL AND i.client_row_id NOT IN ({placeholders})",
                    (company_scope, *keep_ids)
                )
                cursor.execute(
                    f"DELETE FROM inventory_items WHERE company=? AND client_row_id IS NOT NULL AND client_row_id NOT IN ({placeholders})",
                    (company_scope, *keep_ids)
                )
            conn.commit()
            conn.close()
            return {"status": "Saved", "found": True}
        except Exception as e:
            raise RuntimeError(f"Failed to save inventory master tables: {str(e)}")
    except HTTPException:
        close_conn_safely(conn)
        raise
    except Exception as e:
        close_conn_safely(conn)
        log_exception("save_inventory_master", e)
        raise HTTPException(status_code=500, detail=f"Failed to save inventory master: {str(e)}")

def _safe_float(v) -> float:
    try:
        if v is None or v == "":
            return 0.0
        return float(v)
    except Exception:
        return 0.0

def _ensure_space_mono_fonts():
    from reportlab.pdfbase import pdfmetrics
    from reportlab.pdfbase.ttfonts import TTFont

    regular_name = "SpaceMono-Regular"
    semibold_name = "SpaceMono-Bold"

    already = set(pdfmetrics.getRegisteredFontNames())
    if regular_name in already and semibold_name in already:
        return regular_name, semibold_name

    # Local-only lookup. PDF generation is intentionally restricted to Space Mono.
    candidate_dirs = [
        os.path.join(CONFIG_DIR, "fonts"),
        os.path.join(os.getcwd(), "fonts"),
        os.path.join(os.path.dirname(__file__), "fonts"),
        os.path.join(os.getcwd(), "assets", "fonts"),
        os.path.join(os.path.dirname(__file__), "assets", "fonts"),
        os.path.join(os.environ.get("WINDIR", "C:\\Windows"), "Fonts")
    ]

    regular_candidates = [
        "SpaceMono-Regular.ttf",
        "Space Mono Regular.ttf"
    ]
    semibold_candidates = [
        "SpaceMono-Bold.ttf",
        "Space Mono Bold.ttf"
    ]

    regular_path = None
    semibold_path = None

    for d in candidate_dirs:
        for name in regular_candidates:
            p = os.path.join(d, name)
            if os.path.exists(p):
                regular_path = p
                break
        if regular_path:
            break

    for d in candidate_dirs:
        for name in semibold_candidates:
            p = os.path.join(d, name)
            if os.path.exists(p):
                semibold_path = p
                break
        if semibold_path:
            break

    if not regular_path or not semibold_path:
        # Attempt one-time download into config fonts folder to avoid manual setup.
        fonts_dir = os.path.join(CONFIG_DIR, "fonts")
        os.makedirs(fonts_dir, exist_ok=True)
        download_targets = [
            (
                "https://github.com/google/fonts/raw/main/ofl/spacemono/SpaceMono-Regular.ttf",
                os.path.join(fonts_dir, "SpaceMono-Regular.ttf")
            ),
            (
                "https://github.com/google/fonts/raw/main/ofl/spacemono/SpaceMono-Bold.ttf",
                os.path.join(fonts_dir, "SpaceMono-Bold.ttf")
            )
        ]
        try:
            for url, target in download_targets:
                if not os.path.exists(target):
                    urllib.request.urlretrieve(url, target)
        except Exception as e:
            raise RuntimeError(
                "Space Mono font files not found and auto-download failed. "
                f"Place SpaceMono-Regular.ttf and SpaceMono-Bold.ttf under {CONFIG_DIR}\\fonts or assets\\fonts. "
                f"Download error: {str(e)}"
            )

        regular_path = os.path.join(fonts_dir, "SpaceMono-Regular.ttf")
        semibold_path = os.path.join(fonts_dir, "SpaceMono-Bold.ttf")
        if not (os.path.exists(regular_path) and os.path.exists(semibold_path)):
            raise RuntimeError(
                "Space Mono font files are still missing after auto-download. "
                f"Place SpaceMono-Regular.ttf and SpaceMono-Bold.ttf under {CONFIG_DIR}\\fonts or assets\\fonts."
            )

    try:
        pdfmetrics.registerFont(TTFont(regular_name, regular_path))
        pdfmetrics.registerFont(TTFont(semibold_name, semibold_path))
    except Exception as e:
        raise RuntimeError(f"Failed to register Space Mono fonts: {str(e)}")

    return regular_name, semibold_name

def _build_inventory_pdf(
    pdf_path: str,
    month: str,
    fy: str,
    rows: List[InventoryProductRow],
    average_mode: str = "monthly",
    only_reorder: bool = False,
    show_stock_value: bool = False
):
    try:
        import calendar
        from reportlab.lib import colors
        from reportlab.lib.pagesizes import A4
        from reportlab.lib.styles import ParagraphStyle
        from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle
    except Exception as e:
        raise RuntimeError("Missing reportlab dependency. Install with pip install reportlab") from e

    regular_font, semibold_font = _ensure_space_mono_fonts()

    def _extract_month_num(month_label: str) -> int:
        label = (month_label or "").strip().lower()
        month_map = {
            "january": 1, "february": 2, "march": 3, "april": 4, "may": 5, "june": 6,
            "july": 7, "august": 8, "september": 9, "october": 10, "november": 11, "december": 12
        }
        if label in month_map:
            return month_map[label]
        try:
            n = int(label)
            return n if 1 <= n <= 12 else 1
        except Exception:
            return 1

    month_num = _extract_month_num(month)
    max_days = 0
    normalized_rows = []
    for r in rows:
        qty = [_safe_float(x) for x in (r.qty or [])]
        purchase_qty = [_safe_float(x) for x in (getattr(r, "purchase_qty", None) or [])]
        max_days = max(max_days, len(qty), len(purchase_qty))
        normalized_rows.append({
            "name": (r.name or "").strip(),
            "qty": qty,
            "purchase_qty": purchase_qty,
            "min_stock": _safe_float(getattr(r, "min_stock", 0.0)),
            "cost": _safe_float(getattr(r, "cost", 0.0))
        })

    if max_days <= 0:
        max_days = 31

    today = datetime.date.today()
    is_current_month = (month_num == today.month)
    end_day = min(today.day, max_days) if is_current_month else max_days
    start_day = max(1, end_day - 6)
    days = list(range(start_day, end_day + 1))

    month_title = calendar.month_name[month_num] if 1 <= month_num <= 12 else str(month)

    doc = SimpleDocTemplate(
        pdf_path,
        pagesize=A4,
        leftMargin=18,
        rightMargin=18,
        topMargin=16,
        bottomMargin=16
    )

    title_style = ParagraphStyle(
        'InvTitle',
        fontName=semibold_font,
        fontSize=15,
        leading=18,
        textColor=colors.HexColor('#111827')
    )
    sub_style = ParagraphStyle(
        'InvSub',
        fontName=regular_font,
        fontSize=9,
        leading=12,
        textColor=colors.HexColor('#6B7280')
    )
    card_value_style = ParagraphStyle(
        'InvCardVal',
        fontName=semibold_font,
        fontSize=16,
        leading=18,
        textColor=colors.HexColor('#111827')
    )
    card_label_style = ParagraphStyle(
        'InvCardLbl',
        fontName=regular_font,
        fontSize=8,
        leading=10,
        textColor=colors.HexColor('#6B7280')
    )
    metric_label_style = ParagraphStyle(
        'InvMetricLbl',
        fontName=regular_font,
        fontSize=7,
        leading=9,
        textColor=colors.HexColor('#64748B')
    )
    metric_value_style = ParagraphStyle(
        'InvMetricVal',
        fontName=semibold_font,
        fontSize=13,
        leading=15,
        textColor=colors.HexColor('#111827')
    )
    section_style = ParagraphStyle(
        'InvSection',
        fontName=semibold_font,
        fontSize=10,
        leading=12,
        textColor=colors.HexColor('#334155')
    )

    elems = []

    def _purchase_for_day(purchase_list, idx):
        if idx < len(purchase_list) and _safe_float(purchase_list[idx]) > 0:
            return _safe_float(purchase_list[idx])
        return 0.0

    def _format_qty(val):
        return f"{val:,.0f}" if abs(val - int(val)) < 1e-9 else f"{val:,.2f}"

    avg_mode = (average_mode or "monthly").strip().lower()
    if avg_mode not in ("monthly", "last7"):
        avg_mode = "monthly"

    report_rows = []
    grand_total = 0.0
    grand_value = 0.0
    grand_incoming_today = 0.0
    grand_incoming_report = 0.0
    total_daily_outflow = 0.0
    max_avg_days_used = 0
    reorder_count = 0
    for r in normalized_rows:
        qty = r["qty"]
        purchase_qty = r.get("purchase_qty", []) or []
        # Closing/current stock should match the current day shown in the report (end_day),
        # not the last array element (which may include future days as zeros).
        current_idx = min(max(end_day - 1, 0), len(qty) - 1) if qty else -1
        current_qty_base = _safe_float(qty[current_idx] if current_idx >= 0 else 0)
        current_purchase = _purchase_for_day(purchase_qty, current_idx) if current_idx >= 0 else 0.0
        closing_qty = current_qty_base
        row_total = closing_qty
        cost = _safe_float(r.get("cost", 0.0))
        row_value = row_total * cost
        grand_total += row_total
        grand_value += row_value
        grand_incoming_today += current_purchase
        grand_incoming_report += sum(_purchase_for_day(purchase_qty, d - 1) for d in days)
        current_qty = closing_qty

        # Daily outflow (sales/consumption) from stock deltas:
        # if stock drops from previous day, it is treated as movement-out.
        outflow_deltas = []
        for i in range(1, len(qty)):
            prev_qty = _safe_float(qty[i - 1])
            cur_qty = _safe_float(qty[i])
            outflow_deltas.append(max(prev_qty - cur_qty, 0.0))

        days_for_avg = max(max_days - 1, 1)
        avg_base_vals = outflow_deltas
        if avg_mode == "last7":
            days_for_avg = min(7, len(outflow_deltas)) if outflow_deltas else 1
            avg_base_vals = outflow_deltas[-days_for_avg:] if days_for_avg > 0 and outflow_deltas else []

        avg_for_product = (sum(_safe_float(v) for v in avg_base_vals) / max(days_for_avg, 1)) if avg_base_vals else 0.0
        total_daily_outflow += sum(_safe_float(v) for v in avg_base_vals)
        max_avg_days_used = max(max_avg_days_used, days_for_avg)
        min_stock = _safe_float(r.get("min_stock", 0.0))
        threshold = min_stock if min_stock > 0 else avg_for_product
        is_reorder = threshold > 0 and current_qty < threshold

        if is_reorder:
            reorder_count += 1

        report_rows.append({
            "name": r["name"],
            "qty": qty,
            "purchase_qty": purchase_qty,
            "row_total": row_total,
            "row_value": row_value,
            "current_qty": current_qty,
            "avg": avg_for_product,
            "min_stock": min_stock,
            "cost": cost,
            "threshold": threshold,
            "is_reorder": is_reorder,
            "status": "REORDER" if is_reorder else "NORMAL"
        })

    if only_reorder:
        report_rows = [r for r in report_rows if r["is_reorder"]]
        if not report_rows:
            raise RuntimeError("No reorder products found for selected month and rule")

    avg_daily = (total_daily_outflow / max(max_avg_days_used, 1)) if total_daily_outflow > 0 else 0.0

    month_chip = Table([[Paragraph(f"MONTH: {month_title.upper()}", ParagraphStyle(
        'InvMonthChip',
        fontName=semibold_font,
        fontSize=10,
        leading=12,
        textColor=colors.white,
        alignment=1
    ))]], colWidths=[170])
    month_chip.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (0, 0), colors.HexColor('#1f2937')),
        ('BOX', (0, 0), (0, 0), 0.25, colors.HexColor('#111827')),
        ('LEFTPADDING', (0, 0), (0, 0), 8),
        ('RIGHTPADDING', (0, 0), (0, 0), 8),
        ('TOPPADDING', (0, 0), (0, 0), 6),
        ('BOTTOMPADDING', (0, 0), (0, 0), 6),
    ]))
    header_table = Table(
        [[Paragraph("M-Finlogs Inventory Report", title_style), month_chip]],
        colWidths=[A4[0] - doc.leftMargin - doc.rightMargin - 180, 180]
    )
    header_table.setStyle(TableStyle([
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('ALIGN', (1, 0), (1, 0), 'RIGHT'),
        ('LEFTPADDING', (0, 0), (-1, -1), 0),
        ('RIGHTPADDING', (0, 0), (-1, -1), 0),
        ('TOPPADDING', (0, 0), (-1, -1), 0),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 0)
    ]))
    elems.append(header_table)
    elems.append(Spacer(1, 8))

    rows_label = len([r for r in report_rows if r.get("name")]) if only_reorder else len([r for r in normalized_rows if r.get("name")])
    mode_label = "Last 7 Days" if avg_mode == "last7" else "Monthly Average"
    elems.append(Paragraph(f"{fy} | Products: {rows_label} | Current Qty: {grand_total:,.2f} | Days: {start_day}-{end_day} | Rule: {mode_label}", sub_style))
    elems.append(Paragraph(f"Generated: {datetime.datetime.now().strftime('%d %b %Y, %H:%M')}", sub_style))
    elems.append(Spacer(1, 10))

    cards_cells = [[
        [Paragraph(f"{grand_total:,.2f}", card_value_style), Paragraph("Current Quantity", card_label_style)],
        [Paragraph(f"{grand_incoming_report:,.2f}", card_value_style), Paragraph("Purchase In", card_label_style)],
        [Paragraph(f"{avg_daily:,.1f}", card_value_style), Paragraph("Avg Daily Movement", card_label_style)],
        [Paragraph(f"{reorder_count}", card_value_style), Paragraph("Reorder Products", card_label_style)]
    ]]
    card_width = (A4[0] - doc.leftMargin - doc.rightMargin) / 4
    cards_table = Table(cards_cells, colWidths=[card_width] * 4)
    cards_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, -1), colors.HexColor('#F8FAFC')),
        ('BOX', (0, 0), (-1, -1), 0.35, colors.HexColor('#CBD5E1')),
        ('INNERGRID', (0, 0), (-1, -1), 0.25, colors.HexColor('#E2E8F0')),
        ('ALIGN', (0, 0), (-1, -1), 'CENTER'),
        ('LEFTPADDING', (0, 0), (-1, -1), 6),
        ('RIGHTPADDING', (0, 0), (-1, -1), 6),
        ('TOPPADDING', (0, 0), (-1, 0), 8),
        ('BOTTOMPADDING', (0, 0), (-1, 0), 2),
        ('TOPPADDING', (0, 1), (-1, 1), 1),
        ('BOTTOMPADDING', (0, 1), (-1, 1), 8),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE')
    ]))
    elems.append(cards_table)
    elems.append(Spacer(1, 12))

    page_width = A4[0] - doc.leftMargin - doc.rightMargin

    def _format_qty(val):
        return f"{val:,.0f}" if abs(val - int(val)) < 1e-9 else f"{val:,.2f}"

    header = ["Product", "Opening", "Purchase", "Closing"] + [str(d) for d in days]
    data = [header]
    reorder_rows = []
    group_rows = []
    group_index = 1

    for r in report_rows:
        if not r["name"]:
            if len(data) > 1 and data[-1][0] != "":
                data.append([f"Group {group_index}"] + [""] * (len(header) - 1))
                group_rows.append(len(data) - 1)
                group_index += 1
            continue

        qty = r["qty"]
        purchases = r.get("purchase_qty", []) or []
        row_vals = []
        incoming_total = 0.0
        for d in days:
            idx = d - 1
            qv = _safe_float(qty[idx] if idx < len(qty) else 0)
            pv = _purchase_for_day(purchases, idx)
            incoming_total += pv
            cell = _format_qty(qv)
            if pv > 0:
                cell = f"{cell}\n+{_format_qty(pv)}"
            row_vals.append(cell)

        opening_idx = max(start_day - 1, 0)
        opening_qty = _safe_float(qty[opening_idx] if opening_idx < len(qty) else 0)
        current_idx = min(max(end_day - 1, 0), len(qty) - 1) if qty else -1
        closing_base_qty = _safe_float(qty[current_idx] if current_idx >= 0 else 0)
        closing_qty = closing_base_qty

        data.append([
            r["name"][:28],
            _format_qty(opening_qty),
            _format_qty(incoming_total),
            _format_qty(closing_qty)
        ] + row_vals)

        if r["is_reorder"]:
            reorder_rows.append(len(data) - 1)

    if data and data[-1][0].startswith("Group "):
        data.pop()
        group_rows.pop()

    fixed_width = 286
    day_col_width = max(30, min(38, (page_width - fixed_width) / max(len(days), 1)))
    col_widths = [112, 52, 62, 60] + [day_col_width] * len(days)

    table = Table(data, repeatRows=1, colWidths=col_widths)
    style_cmds = [
        ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#F3F4F6')),
        ('TEXTCOLOR', (0, 0), (-1, 0), colors.HexColor('#374151')),
        ('FONTNAME', (0, 0), (-1, 0), semibold_font),
        ('FONTNAME', (0, 1), (-1, -1), regular_font),
        ('FONTSIZE', (0, 0), (-1, 0), 7),
        ('FONTSIZE', (0, 1), (-1, -1), 7),
        ('ALIGN', (0, 0), (0, -1), 'LEFT'),
        ('ALIGN', (1, 0), (-1, -1), 'RIGHT'),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('GRID', (0, 0), (-1, -1), 0.35, colors.HexColor('#D1D5DB')),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, colors.HexColor('#F8FAFC')]),
        ('LEFTPADDING', (0, 0), (0, -1), 6),
        ('RIGHTPADDING', (0, 0), (0, -1), 6),
        ('LEFTPADDING', (1, 0), (-1, -1), 3),
        ('RIGHTPADDING', (1, 0), (-1, -1), 3),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4)
    ]
    current_day_col = 4 + max(0, end_day - start_day)
    style_cmds.extend([
        ('BACKGROUND', (current_day_col, 0), (current_day_col, -1), colors.HexColor('#DBEAFE')),
        ('TEXTCOLOR', (current_day_col, 0), (current_day_col, 0), colors.HexColor('#1E3A8A')),
        ('FONTNAME', (current_day_col, 0), (current_day_col, -1), semibold_font),
        ('LINEBEFORE', (current_day_col, 0), (current_day_col, -1), 0.8, colors.HexColor('#2563EB')),
        ('LINEAFTER', (current_day_col, 0), (current_day_col, -1), 0.8, colors.HexColor('#2563EB'))
    ])

    for ridx in range(1, len(data)):
        for cidx in range(4, len(data[ridx])):
            if "\n+" in str(data[ridx][cidx]):
                style_cmds.append(('BACKGROUND', (cidx, ridx), (cidx, ridx), colors.HexColor('#ECFDF3')))
                style_cmds.append(('TEXTCOLOR', (cidx, ridx), (cidx, ridx), colors.HexColor('#047857')))

    for ridx in group_rows:
        style_cmds.extend([
            ('SPAN', (0, ridx), (-1, ridx)),
            ('BACKGROUND', (0, ridx), (-1, ridx), colors.HexColor('#E5E7EB')),
            ('TEXTCOLOR', (0, ridx), (-1, ridx), colors.HexColor('#475569')),
            ('FONTNAME', (0, ridx), (-1, ridx), semibold_font),
            ('ALIGN', (0, ridx), (-1, ridx), 'LEFT'),
        ])

    for ridx in reorder_rows:
        style_cmds.extend([
            ('TEXTCOLOR', (0, ridx), (3, ridx), colors.HexColor('#B91C1C')),
            ('FONTNAME', (0, ridx), (3, ridx), semibold_font),
            ('BACKGROUND', (0, ridx), (-1, ridx), colors.HexColor('#FEF2F2')),
        ])

    table.setStyle(TableStyle(style_cmds))

    elems.append(Paragraph(f"Daily Stock (Last 7 Days: {start_day}-{end_day})", section_style))
    elems.append(table)
    elems.append(Spacer(1, 6))

    # Per-day stock value summary
    day_value_data = [["Day", "Total Qty", "Stock Value"]]
    for d in days:
        day_qty_total = 0.0
        day_value_total = 0.0
        for r in report_rows:
            qty_list = r.get("qty", [])
            q = _safe_float(qty_list[d - 1] if d - 1 < len(qty_list) else 0)
            c = _safe_float(r.get("cost", 0.0))
            day_qty_total += q
            day_value_total += q * c
        day_value_data.append([
            f"{d}",
            f"{day_qty_total:,.2f}",
            f"{day_value_total:,.2f}"
        ])

    elems.append(Spacer(1, 8))
    elems.append(Paragraph("Per-Day Stock Value", section_style))
    value_table = Table(day_value_data, repeatRows=1, colWidths=[90, 130, 150])
    value_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#f3f4f6')),
        ('TEXTCOLOR', (0, 0), (-1, 0), colors.HexColor('#374151')),
        ('FONTNAME', (0, 0), (-1, 0), semibold_font),
        ('FONTNAME', (0, 1), (-1, -1), regular_font),
        ('FONTSIZE', (0, 0), (-1, 0), 7),
        ('FONTSIZE', (0, 1), (-1, -1), 7),
        ('ALIGN', (0, 0), (0, -1), 'CENTER'),
        ('ALIGN', (1, 0), (-1, -1), 'RIGHT'),
        ('GRID', (0, 0), (-1, -1), 0.35, colors.HexColor('#d1d5db')),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, colors.HexColor('#F9FAFB')]),
        ('LEFTPADDING', (0, 0), (-1, -1), 6),
        ('RIGHTPADDING', (0, 0), (-1, -1), 6),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4)
    ]))
    elems.append(value_table)
    elems.append(Spacer(1, 6))

    doc.build(elems)

def _get_inventory_pdf_day_range(rows: List[InventoryProductRow], month: str):
    try:
        import calendar
        month_map = {
            "january": 1, "february": 2, "march": 3, "april": 4, "may": 5, "june": 6,
            "july": 7, "august": 8, "september": 9, "october": 10, "november": 11, "december": 12
        }
        label = (month or "").strip().lower()
        month_num = month_map.get(label)
        if month_num is None:
            month_num = int(label) if label.isdigit() else datetime.date.today().month
        month_num = month_num if 1 <= month_num <= 12 else datetime.date.today().month
        max_days = max(
            [
                max(
                    len(r.qty or []),
                    len(getattr(r, "purchase_qty", None) or [])
                )
                for r in (rows or [])
            ] or [31]
        )
        max_days = max_days if max_days > 0 else 31
        today = datetime.date.today()
        end_day = min(today.day, max_days) if month_num == today.month else max_days
        start_day = max(1, end_day - 6)
        month_slug = calendar.month_name[month_num] if 1 <= month_num <= 12 else str(month)
        return month_slug, start_day, end_day
    except Exception:
        safe_month = (month or "Month").strip() or "Month"
        return safe_month, 1, 1

def _inventory_pdf_filename(month: str, fy: str, rows: List[InventoryProductRow]) -> str:
    month_label, start_day, end_day = _get_inventory_pdf_day_range(rows, month)
    raw = f"Inventory_{month_label}_Days_{start_day}-{end_day}_{fy}.pdf"
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", raw).strip("_")

def _send_mail_with_pdf(
    smtp_host: str,
    smtp_port: int,
    sender: str,
    sender_password: str,
    recipients: List[str],
    msg: EmailMessage
):
    host = (smtp_host or "").strip()
    if not host:
        raise HTTPException(status_code=400, detail="SMTP host is required")
    if smtp_port <= 0:
        raise HTTPException(status_code=400, detail=f"SMTP port is invalid: {smtp_port}")
    if not recipients:
        raise HTTPException(status_code=400, detail="At least one recipient email is required")

    if smtp_port == 465:
        with smtplib.SMTP_SSL(host, smtp_port, timeout=25) as smtp:
            smtp.login(sender, sender_password)
            smtp.send_message(msg, from_addr=sender, to_addrs=recipients)
        return

    with smtplib.SMTP(host, smtp_port, timeout=25) as smtp:
        smtp.ehlo()
        smtp.starttls()
        smtp.ehlo()
        smtp.login(sender, sender_password)
        smtp.send_message(msg, from_addr=sender, to_addrs=recipients)

@app.post("/report/inventory/email-pdf")
def send_inventory_pdf_mail(req: InventoryPdfMailRequest, authorization: Optional[str] = Header(None)):
    require_authenticated(authorization)
    month = (req.month or "").strip()
    if not month:
        raise HTTPException(status_code=400, detail="Month is required")

    if not req.rows:
        raise HTTPException(status_code=400, detail="No inventory rows provided")

    to_addr = (req.to_email or "").strip()
    if not to_addr:
        raise HTTPException(status_code=400, detail="Recipient email is required")

    sender = (req.sender_email or "").strip()
    if not sender:
        raise HTTPException(status_code=400, detail="Sender email is required")

    if not (req.sender_password or "").strip():
        raise HTTPException(status_code=400, detail="Sender app password is required")

    fy = (req.financial_year or get_selected_financial_year()).strip()
    subject = (req.subject or f"Inventory Report - {month} ({fy})").strip()
    notes = (req.notes or "").strip()

    temp_pdf = None
    try:
        fd, temp_pdf = tempfile.mkstemp(prefix="inventory_report_", suffix=".pdf")
        os.close(fd)

        _build_inventory_pdf(
            temp_pdf,
            month,
            fy,
            req.rows,
            average_mode=req.average_mode,
            only_reorder=req.only_reorder,
            show_stock_value=req.show_stock_value
        )

        msg = EmailMessage()
        msg["Subject"] = subject
        msg["From"] = sender
        msg["To"] = to_addr
        cc_clean = (req.cc_email or "").strip()
        if cc_clean:
            msg["Cc"] = cc_clean

        body = [
            "Please find attached the Inventory Management report.",
            "",
            f"Financial Year: {fy}",
            f"Month: {month}",
            f"Products: {len(req.rows)}",
            f"Generated On: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
        ]
        if notes:
            body += ["", "Notes:", notes]
        msg.set_content("\n".join(body))

        with open(temp_pdf, "rb") as f:
            pdf_bytes = f.read()
        filename = _inventory_pdf_filename(month, fy, req.rows)
        msg.add_attachment(pdf_bytes, maintype="application", subtype="pdf", filename=filename)

        recipients = [x.strip() for x in (to_addr + ("," + cc_clean if cc_clean else "")).split(",") if x.strip()]
        _send_mail_with_pdf(
            req.smtp_host,
            int(req.smtp_port),
            sender,
            req.sender_password,
            recipients,
            msg
        )

        return {
            "status": "Sent",
            "detail": f"Inventory PDF mailed successfully to {to_addr}",
            "pdf_name": filename
        }
    except RuntimeError as e:
        raise HTTPException(status_code=400, detail=str(e))
    except HTTPException:
        raise
    except smtplib.SMTPAuthenticationError:
        raise HTTPException(
            status_code=400,
            detail=(
                "SMTP authentication failed. Use the correct sender email and app password. "
                "For Gmail, enable 2-Step Verification and create an App Password."
            )
        )
    except smtplib.SMTPException as e:
        raise HTTPException(status_code=400, detail=f"SMTP error: {str(e)}")
    except OSError as e:
        raise HTTPException(
            status_code=400,
            detail=f"SMTP connection failed for {req.smtp_host}:{req.smtp_port}: {str(e)}"
        )
    except Exception as e:
        log_exception("send_inventory_pdf_mail", e)
        raise HTTPException(status_code=500, detail=f"Failed to send inventory report mail: {str(e)}")
    finally:
        if temp_pdf and os.path.exists(temp_pdf):
            try:
                os.remove(temp_pdf)
            except Exception:
                pass

@app.post("/report/inventory/pdf-preview")
def preview_inventory_pdf(req: InventoryPdfPreviewRequest, authorization: Optional[str] = Header(None)):
    require_authenticated(authorization)
    month = (req.month or "").strip()
    if not month:
        raise HTTPException(status_code=400, detail="Month is required")

    if not req.rows:
        raise HTTPException(status_code=400, detail="No inventory rows provided")

    fy = (req.financial_year or get_selected_financial_year()).strip()

    temp_pdf = None
    try:
        fd, temp_pdf = tempfile.mkstemp(prefix="inventory_preview_", suffix=".pdf")
        os.close(fd)

        _build_inventory_pdf(
            temp_pdf,
            month,
            fy,
            req.rows,
            average_mode=req.average_mode,
            only_reorder=req.only_reorder,
            show_stock_value=req.show_stock_value
        )

        with open(temp_pdf, "rb") as f:
            pdf_bytes = f.read()

        filename = _inventory_pdf_filename(month, fy, req.rows)
        headers = {"Content-Disposition": f'inline; filename="{filename}"'}
        return Response(content=pdf_bytes, media_type="application/pdf", headers=headers)
    except RuntimeError as e:
        raise HTTPException(status_code=400, detail=str(e))
    except HTTPException:
        raise
    except Exception as e:
        log_exception("preview_inventory_pdf", e)
        raise HTTPException(status_code=500, detail=f"Failed to generate preview PDF: {str(e)}")
    finally:
        if temp_pdf and os.path.exists(temp_pdf):
            try:
                os.remove(temp_pdf)
            except Exception:
                pass

def set_setting(key: str, value: float):
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("SELECT COUNT(*) FROM app_settings WHERE setting_key=?", (key,))
    exists = cursor.fetchone()[0] > 0
    if exists:
        cursor.execute("UPDATE app_settings SET setting_value=? WHERE setting_key=?", (str(value), key))
    else:
        cursor.execute("INSERT INTO app_settings (setting_key, setting_value) VALUES (?, ?)", (key, str(value)))
    conn.close()

@app.get("/companies")
def list_companies():
    # In SQL Server, we store company info in app_settings table
    # For simplicity, return the current company
    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute("SELECT setting_value FROM app_settings WHERE setting_key='company_name'")
        row = cursor.fetchone()
        conn.close()
        company_name = row[0] if row and row[0] else "default"
        return [{"name": company_name, "key": normalize_company(company_name)}]
    except Exception as err:
        log_exception("list_companies", err)
        return [{"name": "default", "key": "default"}]

@app.post("/companies")
def create_company(req: CompanyCreateRequest, authorization: Optional[str] = Header(None)):
    require_admin(require_authenticated(authorization))
    name = (req.name or "").strip()
    if not name:
        raise HTTPException(status_code=400, detail="Company name required")
    init_db(name)
    return {"status": "Created"}

@app.post("/company/select")
def select_company(req: CompanySelectRequest, authorization: Optional[str] = Header(None)):
    require_authenticated(authorization)
    global current_company
    name = (req.name or "").strip()
    if not name:
        raise HTTPException(status_code=400, detail="Company name required")
    current_company = name
    init_db(name)
    return {"status": "Selected", "company": name}

@app.get("/financial-years")
def list_financial_years():
    conn = get_db_connection()
    cursor = conn.cursor()
    selected_fy = get_selected_financial_year(cursor)
    ensure_financial_year_column(cursor)
    cursor.execute("SELECT DISTINCT financial_year FROM transactions WHERE financial_year IS NOT NULL ORDER BY financial_year DESC")
    years = [str(r[0]) for r in cursor.fetchall() if r[0]]
    conn.close()

    # Ensure current and selected FY are available in list
    current_fy = financial_year_for_date(datetime.date.today())
    merged = []
    for y in [selected_fy, current_fy] + years:
        if y and y not in merged:
            merged.append(y)
    merged.sort(reverse=True)

    return {"years": merged, "selected": selected_fy}

@app.post("/financial-year/select")
def select_financial_year(req: FinancialYearSelectRequest):
    global current_financial_year
    selected = (req.year or "").strip()
    if not selected:
        raise HTTPException(status_code=400, detail="Financial year is required")
    try:
        parse_financial_year(selected)
    except ValueError as e:
        raise HTTPException(status_code=400, detail=str(e))

    current_financial_year = selected
    set_text_setting("active_financial_year", selected)
    return {"status": "Selected", "financial_year": selected}

class PartyCreate(BaseModel):
    name: str
    ptype: str
    credit: bool

@app.post("/party")
def create_party(party: PartyCreate, authorization: Optional[str] = Header(None)):
    require_authenticated(authorization)
    conn = get_db_connection()
    cursor = conn.cursor()
    
    # Logic Fix: Only one Bank allowed
    if party.ptype == "Bank":
        count = cursor.execute("SELECT COUNT(*) FROM parties WHERE type = 'Bank'").fetchone()[0]
        if count > 0:
             conn.close()
             raise HTTPException(status_code=400, detail="Only one Bank account is allowed.")

    try:
        get_or_create_party_id(cursor, party.name, party.ptype, party.credit)
        conn.close()
        return {"status": "Party Created"}
    except Exception as e:
        log_exception("create_party", e)
        conn.close()
        return {"status": "Error", "detail": str(e)}

@app.post("/transaction")
def add_transaction(date: str, bill_no: str, party: str, txn_type: str, mode: str, amount: float, authorization: Optional[str] = Header(None)):
    require_authenticated(authorization)
    conn = get_db_connection()
    try:
        cursor = conn.cursor()
        ensure_financial_year_column(cursor)
        cursor.execute("SELECT party_id FROM parties WHERE normalized_name=?", (normalize_party_key(party),))
        row = cursor.fetchone()
        if not row:
            raise HTTPException(status_code=404, detail="Party not found")
            
        party_id = row[0]

        txn_date = parse_date_str(date) or datetime.date.today()
        txn_fy = financial_year_for_date(txn_date)

        cursor.execute(
            "INSERT INTO transactions (txn_date, bill_no, party_id, txn_type, payment_mode, financial_year, amount) VALUES (?, ?, ?, ?, ?, ?, ?)",
            (date, bill_no, party_id, txn_type, mode, txn_fy, amount)
        )
        conn.close()
        invalidate_report_cache()
        return {"status": "Transaction Added"}
    except HTTPException:
        close_conn_safely(conn)
        raise
    except Exception as e:
        close_conn_safely(conn)
        log_exception("add_transaction", e)
        raise HTTPException(status_code=500, detail=f"Unable to add transaction: {e}")

@app.get("/ledger/{party:path}")
def get_ledger(party: str, start: str = None, end: str = None):
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        cursor.execute("SELECT party_id FROM parties WHERE normalized_name=?", (party,))
        row = cursor.fetchone()
        if not row:
            conn.close()
            # If party doesn't exist, return empty ledger instead of crashing
            return {
                "data": [],
                "opening_balance": 0.0,
                "period_start": start,
                "period_end": end,
                "financial_year": get_selected_financial_year(cursor)
            }
            
        party_id = row[0]

        selected_fy = get_selected_financial_year(cursor)

        start_date = parse_date_str(start) if start else None
        end_date = parse_date_str(end) if end else None

        if start_date and end_date and start_date > end_date:
            conn.close()
            return {
                "data": [],
                "opening_balance": 0.0,
                "period_start": start_date.isoformat(),
                "period_end": end_date.isoformat(),
                "financial_year": selected_fy
            }

        # Opening balance is cumulative before start date (supports April 1 opening = March 31 closing).
        # Include common legacy spelling variant "Reciept" and case/space variations.
        opening_balance = 0.0
        if start_date:
            opening_query = """
                SELECT SUM(
                    CASE
                        WHEN UPPER(LTRIM(RTRIM(txn_type))) = 'SALE' THEN amount
                        WHEN UPPER(LTRIM(RTRIM(txn_type))) IN ('RECEIPT', 'RECIEPT', 'SALE RETURN') THEN -amount
                        ELSE 0
                    END
                )
                FROM transactions
                WHERE party_id=? AND txn_date < ?
            """
            cursor.execute(opening_query, (party_id, start_date.isoformat()))
            opening_result = cursor.fetchone()
            opening_balance = float(opening_result[0] or 0) if opening_result else 0
        balance = opening_balance

        # Get filtered transactions for display
        query = "SELECT txn_id, txn_date, bill_no, txn_type, payment_mode, amount FROM transactions WHERE party_id=?"
        params = [party_id]

        if start_date:
            query += " AND txn_date >= ?"
            params.append(start_date.isoformat())
        if end_date:
            query += " AND txn_date <= ?"
            params.append(end_date.isoformat())

        query += " ORDER BY txn_date, txn_id"
        cursor.execute(query, params)
        data = cursor.fetchall()
        conn.close() 

        ledger = []

        for dim, d, b, t, m, a in data:
            # Align with outstanding report logic:
            # Debit: Sale increases receivable
            # Credit: Receipt and Sale Return decrease receivable
            # Other transactions (Expense, Purchase) should not affect customer receivable balance
            
            txn_type_norm = str(t or '').strip().lower()

            if txn_type_norm == "sale":
                balance += float(a)
            elif txn_type_norm in ["receipt", "reciept", "sale return"]:
                balance -= float(a) 
            # Note: Expense, Purchase, and other types are NOT included in customer receivable balance

            ledger.append({
                "id": dim, # This is txn_id
                "date": str(d),
                "bill_no": b if b else "",
                "type": t,
                "mode": m,
                "amount": float(a),
                "balance": float(balance),
                "financial_year": selected_fy
            })

        return {
            "data": ledger,
            "opening_balance": float(opening_balance),
            "period_start": start_date.isoformat() if start_date else None,
            "period_end": end_date.isoformat() if end_date else None,
            "financial_year": selected_fy
        }
    except Exception as e:
        log_exception("get_ledger", e)
        try: conn.close()
        except: pass
        return {
            "data": [],
            "opening_balance": 0.0,
            "period_start": start,
            "period_end": end,
            "financial_year": current_financial_year or financial_year_for_date(datetime.date.today())
        }


@app.get("/parties")
def get_parties():
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("SELECT name, type FROM parties ORDER BY name")
    data = [{"name": r[0], "type": r[1]} for r in cursor.fetchall()]
    conn.close()
    return data
@app.get("/transactions")
def get_transactions(page: int = 1, limit: int = 50, days: int = 30, from_date: Optional[str] = None, to_date: Optional[str] = None):
    conn = get_db_connection()
    cursor = conn.cursor()

    selected_fy, fy_start, fy_end = get_financial_year_bounds(cursor)

    where_clauses = []
    params = []

    # Always restrict by selected financial year
    where_clauses.append("t.txn_date >= ?")
    params.append(fy_start.isoformat())
    where_clauses.append("t.txn_date <= ?")
    params.append(fy_end.isoformat())

    if from_date:
        where_clauses.append("t.txn_date >= ?")
        params.append(from_date)
    if to_date:
        where_clauses.append("t.txn_date <= ?")
        params.append(to_date)

    if not where_clauses and days and days > 0:
        where_clauses.append("t.txn_date >= DATEADD(day, ?, CAST(GETDATE() AS date))")
        params.append(-days)

    where_sql = f"WHERE {' AND '.join(where_clauses)}" if where_clauses else ""
    
    # Get total count
    cursor.execute(f"SELECT COUNT(*) FROM transactions t {where_sql}", params)
    total = cursor.fetchone()[0]
    
    offset = (page - 1) * limit
    cursor.execute(f"""
        SELECT t.txn_id, t.txn_date, t.bill_no, p.name, t.txn_type, t.payment_mode, t.amount
        FROM transactions t
        JOIN parties p ON t.party_id = p.party_id
        {where_sql}
        ORDER BY t.txn_date DESC, t.txn_id DESC
        OFFSET ? ROWS FETCH NEXT ? ROWS ONLY
    """, (*params, offset, limit))
    rows = cursor.fetchall()
    conn.close()
    
    total_pages = (total + limit - 1) // limit

    return {
        "transactions": [
        {
            "id": r[0],
            "date": str(r[1]),
            "bill_no": r[2] if r[2] else "",
            "party": r[3],
            "type": r[4],
            "mode": r[5],
            "amount": float(r[6])
        }
        for r in rows
        ],
        "page": page,
        "limit": limit,
        "total": total,
        "total_pages": total_pages
    }

@app.get("/transactions/by-date")
def get_transactions_by_date(date: str):
    conn = get_db_connection()
    cursor = conn.cursor()
    _, fy_start, fy_end = get_financial_year_bounds(cursor)
    cursor.execute("""
        SELECT t.txn_id, t.txn_date, t.bill_no, p.name, t.txn_type, t.payment_mode, t.amount
        FROM transactions t
        JOIN parties p ON t.party_id = p.party_id
        WHERE t.txn_date = ?
          AND t.txn_date >= ?
          AND t.txn_date <= ?
        ORDER BY t.txn_date DESC, t.txn_id DESC
    """, (date, fy_start.isoformat(), fy_end.isoformat()))
    rows = cursor.fetchall()
    conn.close()

    return [
        {
            "id": r[0],
            "date": str(r[1]),
            "bill_no": r[2] if r[2] else "",
            "party": r[3],
            "type": r[4],
            "mode": r[5],
            "amount": float(r[6])
        }
        for r in rows
    ]

@app.get("/transaction/{txn_id}")
def get_single_transaction(txn_id: int):
    """Get a single transaction by ID for efficient edit modal loading"""
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("""
        SELECT t.txn_id, t.txn_date, t.bill_no, p.name, t.txn_type, t.payment_mode, t.amount
        FROM transactions t
        JOIN parties p ON t.party_id = p.party_id
        WHERE t.txn_id = ?
    """, (txn_id,))
    row = cursor.fetchone()
    conn.close()
    
    if not row:
        raise HTTPException(status_code=404, detail="Transaction not found")
    
    return {
        "id": row[0],
        "date": str(row[1]),
        "bill_no": row[2] if row[2] else "",
        "party": row[3],
        "type": row[4],
        "mode": row[5],
        "amount": float(row[6])
    }

@app.get("/summary/daily")
def get_daily_summary():
    conn = get_db_connection()
    cursor = conn.cursor()
    _, fy_start, fy_end = get_financial_year_bounds(cursor)
    cursor.execute("""
        SELECT txn_date, payment_mode, txn_type, SUM(amount)
        FROM transactions
        WHERE payment_mode IN ('Cash', 'Bank', 'UPI', 'GPay', 'GPAY', 'Google Pay', 'GooglePay')
          AND txn_date >= ?
          AND txn_date <= ?
        GROUP BY txn_date, payment_mode, txn_type
        ORDER BY txn_date DESC
    """, (fy_start.isoformat(), fy_end.isoformat()))
    rows = cursor.fetchall()
    conn.close()

    summary = {}
    
    for date, mode, ttype, amount in rows:
        d_str = str(date)
        if d_str not in summary:
            summary[d_str] = {
                "date": d_str,
                "cash_in": 0, "cash_out": 0,
                "bank_in": 0, "bank_out": 0,
                "upi_in": 0, "upi_out": 0
            }
        
        tnorm = str(ttype or "").strip().upper()
        # Logic: Sale/Receipt = In, Expense/Sale Return/Return = Out
        is_in = tnorm in ["SALE", "RECEIPT", "RECIEPT"]
        val = float(amount)

        if mode == "Cash":
            if is_in: summary[d_str]["cash_in"] += val
            else: summary[d_str]["cash_out"] += val
        elif mode == "Bank":
            if is_in: summary[d_str]["bank_in"] += val
            else: summary[d_str]["bank_out"] += val
        elif mode == "UPI":
            if is_in: summary[d_str]["upi_in"] += val
            else: summary[d_str]["upi_out"] += val

    return list(summary.values())

@app.get("/report/mode/{mode}")
def get_mode_report(mode: str):
    conn = get_db_connection()
    cursor = conn.cursor()
    _, fy_start, fy_end = get_financial_year_bounds(cursor)
    if mode.lower() == "bank":
        cursor.execute("""
            SELECT t.txn_date, t.bill_no, p.name, t.txn_type, t.amount
            FROM transactions t
            JOIN parties p ON t.party_id = p.party_id
            WHERE t.payment_mode IN ('Bank', 'UPI', 'GPay', 'GPAY', 'Google Pay', 'GooglePay')
              AND t.txn_date >= ?
              AND t.txn_date <= ?
            ORDER BY t.txn_date
        """, (fy_start.isoformat(), fy_end.isoformat()))
    else:
        cursor.execute("""
            SELECT t.txn_date, t.bill_no, p.name, t.txn_type, t.amount
            FROM transactions t
            JOIN parties p ON t.party_id = p.party_id
            WHERE t.payment_mode = ?
              AND t.txn_date >= ?
              AND t.txn_date <= ?
            ORDER BY t.txn_date
        """, (mode, fy_start.isoformat(), fy_end.isoformat()))
    rows = cursor.fetchall()
    conn.close()
    
    # Calculate running balance for the account (Cash/Bank/UPI)
    # In = Sale/Receipt (+), Out = Expense (-)
    balance = 0
    result = []
    
    for r in rows:
        date, bill, party, ttype, amount = r
        amt = float(amount)
        if ttype in ["Sale", "Receipt"]:
            balance += amt
            debit = amt  # Debit the Cash Account (Increase Asset)
            credit = 0
        else:
            balance -= amt
            debit = 0
            credit = amt # Credit the Cash Account (Decrease Asset)
            
        result.append({
            "date": str(date),
            "bill_no": bill if bill else "",
            "party": party,
            "type": ttype,
            "debit": debit,   # In
            "credit": credit, # Out
            "balance": balance
        })
    return result

@app.get("/report/type/{txn_type}")
def get_type_report(txn_type: str):
    conn = get_db_connection()
    cursor = conn.cursor()
    _, fy_start, fy_end = get_financial_year_bounds(cursor)
    cursor.execute("""
        SELECT t.txn_date, t.bill_no, p.name, t.payment_mode, t.amount
        FROM transactions t
        JOIN parties p ON t.party_id = p.party_id
        WHERE t.txn_type = ?
          AND t.txn_date >= ?
          AND t.txn_date <= ?
        ORDER BY t.txn_date
    """, (txn_type, fy_start.isoformat(), fy_end.isoformat()))
    rows = cursor.fetchall()
    conn.close()
    
    result = []
    total = 0
    for r in rows:
        total += float(r[4])
        result.append({
            "date": str(r[0]),
            "bill_no": r[1] if r[1] else "",
            "party": r[2],
            "mode": r[3],
            "amount": float(r[4])
        })
    return result

@app.get("/report/purchase/monthly")
def get_purchase_monthly_report(start: Optional[str] = None, end: Optional[str] = None, days: int = 30):
    start_date, end_date = resolve_date_range(start, end, days)
    conn = get_db_connection()
    cursor = conn.cursor()
    selected_fy, fy_start, fy_end = get_financial_year_bounds(cursor)
    if start_date < fy_start:
        start_date = fy_start
    if end_date > fy_end:
        end_date = fy_end
    cache_key = (current_company or "default", selected_fy, "purchase_monthly", str(start_date), str(end_date))
    cached = cache_get(cache_key)
    if cached is not None:
        conn.close()
        return cached

    cursor.execute(
        """
        SELECT
            CONVERT(varchar(7), t.txn_date, 23) AS month_key,
            SUM(t.amount) AS total_amount
        FROM transactions t
        JOIN parties p ON t.party_id = p.party_id
                WHERE t.txn_date BETWEEN ? AND ?
                    AND t.txn_type = 'Sale'
                    AND p.type IN ('Customer', 'Credit Customer')
        GROUP BY CONVERT(varchar(7), t.txn_date, 23)
        ORDER BY month_key
        """,
        (start_date, end_date)
    )
    rows = cursor.fetchall()
    conn.close()

    result = [
        {
            "month": r[0],
            "total_amount": float(r[1] or 0)
        }
        for r in rows
    ]
    cache_set(cache_key, result)
    return result

@app.get("/report/purchase/supplier")
def get_purchase_supplier_report(start: Optional[str] = None, end: Optional[str] = None, days: int = 30, party: Optional[str] = None):
    start_date, end_date = resolve_date_range(start, end, days)
    conn = get_db_connection()
    cursor = conn.cursor()
    selected_fy, fy_start, fy_end = get_financial_year_bounds(cursor)
    if start_date < fy_start:
        start_date = fy_start
    if end_date > fy_end:
        end_date = fy_end
    cache_key = (current_company or "default", selected_fy, "purchase_supplier", str(start_date), str(end_date), (party or "").strip())
    cached = cache_get(cache_key)
    if cached is not None:
        conn.close()
        return cached

    base_query = """
        SELECT
            p.name,
            SUM(t.amount) AS total_amount
        FROM transactions t
        JOIN parties p ON t.party_id = p.party_id
        WHERE t.txn_date BETWEEN ? AND ?
          AND t.txn_type = 'Sale'
          AND p.type IN ('Customer', 'Credit Customer')
    """
    params = [start_date, end_date]
    if party and party.strip():
        base_query += " AND p.name = ?"
        params.append(party.strip())
    base_query += " GROUP BY p.name ORDER BY p.name"
    cursor.execute(base_query, params)
    rows = cursor.fetchall()
    conn.close()

    result = [
        {
            "party": r[0],
            "total_amount": float(r[1] or 0)
        }
        for r in rows
    ]
    cache_set(cache_key, result)
    return result
@app.get("/report/outstanding")
def get_outstanding_report():
    conn = get_db_connection()
    cursor = conn.cursor()
    _, fy_start, fy_end = get_financial_year_bounds(cursor)
    cursor.execute("""
        SELECT p.name, 
               SUM(CASE WHEN UPPER(LTRIM(RTRIM(t.txn_type))) = 'SALE' THEN t.amount ELSE 0 END) as sales,
               SUM(CASE WHEN UPPER(LTRIM(RTRIM(t.txn_type))) IN ('RECEIPT', 'RECIEPT', 'SALE RETURN') THEN t.amount ELSE 0 END) as credits,
               MAX(CASE WHEN UPPER(LTRIM(RTRIM(t.txn_type))) IN ('RECEIPT', 'RECIEPT') THEN t.txn_date END) as last_receipt_date,
               MIN(CASE WHEN UPPER(LTRIM(RTRIM(t.txn_type))) = 'SALE' THEN t.txn_date END) as first_sale_date
        FROM parties p
        LEFT JOIN transactions t ON p.party_id = t.party_id
        WHERE p.type = 'Credit Customer'
          AND t.txn_date >= ?
          AND t.txn_date <= ?
        GROUP BY p.name
    """, (fy_start.isoformat(), fy_end.isoformat()))
    rows = cursor.fetchall()
    conn.close()

    outstanding = []
    total_outstanding = 0.0
    high_count = 0
    critical_count = 0
    high_amount = 0.0
    critical_amount = 0.0
    max_days_unpaid = 0
    today = datetime.date.today()

    for name, sales, credits, last_receipt_date, first_sale_date in rows:
        if name.strip().lower() == "customer":
            continue
        s = float(sales or 0)
        c = float(credits or 0)
        balance = s - c
        if balance > 0:
            days_anchor = last_receipt_date or first_sale_date
            days_unpaid = (today - days_anchor).days if days_anchor else 0
            max_days_unpaid = max(max_days_unpaid, days_unpaid)

            if days_unpaid >= 30:
                risk_level = "critical"
                critical_count += 1
                critical_amount += balance
            elif days_unpaid >= 15:
                risk_level = "high"
                high_count += 1
                high_amount += balance
            else:
                risk_level = "normal"

            outstanding.append({
                "party": name,
                "balance": balance,
                "last_receipt_date": str(last_receipt_date) if last_receipt_date else None,
                "days_unpaid": int(days_unpaid),
                "risk_level": risk_level,
                "risk_rank": 2 if risk_level == "critical" else (1 if risk_level == "high" else 0)
            })
            total_outstanding += balance
    
    return {
        "data": outstanding,
        "total": total_outstanding,
        "summary": {
            "high_count": high_count,
            "critical_count": critical_count,
            "high_amount": high_amount,
            "critical_amount": critical_amount,
            "max_days_unpaid": max_days_unpaid
        }
    }

@app.get("/report/outstanding/party")
def get_outstanding_party_detail(party: str):
    conn = get_db_connection()
    cursor = conn.cursor()
    _, fy_start, fy_end = get_financial_year_bounds(cursor)

    normalized = normalize_party_key(party)
    cursor.execute("SELECT COUNT(*) FROM parties WHERE normalized_name=?", (normalized,))
    row = cursor.fetchone()
    if not row or int(row[0] or 0) == 0:
        conn.close()
        raise HTTPException(status_code=404, detail="Party not found")

    cursor.execute(
        """
        SELECT t.txn_date, t.bill_no, t.txn_type, t.payment_mode, t.amount, t.txn_id
        FROM transactions t
        INNER JOIN parties p ON p.party_id = t.party_id
        WHERE p.normalized_name = ?
          AND t.txn_date >= ?
          AND t.txn_date <= ?
        ORDER BY t.txn_date ASC, t.txn_id ASC
        """,
        (normalized, fy_start.isoformat(), fy_end.isoformat())
    )
    ledger_rows = cursor.fetchall()

    cursor.execute(
        """
        SELECT
            SUM(CASE WHEN UPPER(LTRIM(RTRIM(t.txn_type))) = 'SALE' THEN t.amount ELSE 0 END),
            SUM(CASE WHEN UPPER(LTRIM(RTRIM(t.txn_type))) IN ('RECEIPT', 'RECIEPT', 'SALE RETURN') THEN t.amount ELSE 0 END),
            MAX(CASE WHEN UPPER(LTRIM(RTRIM(t.txn_type))) IN ('RECEIPT', 'RECIEPT', 'SALE RETURN') THEN t.txn_date END)
        FROM transactions t
        INNER JOIN parties p ON p.party_id = t.party_id
        WHERE p.normalized_name = ?
          AND t.txn_date >= ?
          AND t.txn_date <= ?
        """,
        (normalized, fy_start.isoformat(), fy_end.isoformat())
    )
    sums = cursor.fetchone()

    sales = float((sums[0] if sums else 0) or 0)
    receipts = float((sums[1] if sums else 0) or 0)
    outstanding = sales - receipts
    last_receipt = sums[2] if sums else None
    days_unpaid = (datetime.date.today() - last_receipt).days if last_receipt else 0

    running_balance = 0.0
    ledger_with_balance = []
    for d, bill_no, txn_type, payment_mode, amount, txn_id in ledger_rows:
        tnorm = str(txn_type or '').strip().lower()
        amt = float(amount or 0)
        if tnorm == 'sale':
            running_balance += amt
        elif tnorm in ('receipt', 'reciept', 'sale return'):
            running_balance -= amt

        ledger_with_balance.append({
            "date": str(d),
            "bill_no": bill_no or "",
            "type": txn_type,
            "mode": payment_mode,
            "amount": amt,
            "balance": float(running_balance),
            "txn_id": int(txn_id)
        })

    recent_ledger = list(reversed(ledger_with_balance[-4:]))
    closing_balance = float(running_balance)

    suggested = "Contact politely and confirm payment timeline"
    if days_unpaid >= 30:
        suggested = "Critical follow-up: call today and pause new credit until settlement"
    elif days_unpaid >= 15:
        suggested = "High-risk follow-up: share statement and ask for partial payment"

    conn.close()

    return {
        "party": party,
        "outstanding": outstanding,
        "days_unpaid": days_unpaid,
        "last_receipt_date": str(last_receipt) if last_receipt else None,
        "suggested_action": suggested,
        "closing_balance": closing_balance,
        "recent_ledger": recent_ledger
    }

@app.get("/report/trial-balance")
def get_trial_balance():
    conn = get_db_connection()
    cursor = conn.cursor()
    _, fy_start, fy_end = get_financial_year_bounds(cursor)

    cursor.execute(
        """
        SELECT
            payment_mode,
            txn_type,
            SUM(amount) AS total_amount
        FROM transactions
        WHERE txn_date >= ?
          AND txn_date <= ?
        GROUP BY payment_mode, txn_type
        """,
        (fy_start.isoformat(), fy_end.isoformat())
    )
    txn_rows = cursor.fetchall()

    mode_in = {"cash": 0.0, "bank": 0.0, "upi": 0.0}
    mode_out = {"cash": 0.0, "bank": 0.0, "upi": 0.0}
    bank_modes = {'bank', 'gpay', 'gpay', 'google pay', 'googlepay'}
    for payment_mode, txn_type, total_amount in txn_rows:
        m = str(payment_mode or '').strip().lower()
        t = str(txn_type or '').strip().lower()
        amt = float(total_amount or 0)
        key = 'bank' if m in bank_modes else ('upi' if m == 'upi' else 'cash')
        if t in ('sale', 'receipt', 'reciept'):
            mode_in[key] += amt
        elif t in ('expense', 'sale return', 'sales return', 'return'):
            mode_out[key] += amt

    cash_bal = mode_in['cash'] - mode_out['cash']
    bank_bal = mode_in['bank'] - mode_out['bank']
    upi_bal = mode_in['upi'] - mode_out['upi']

    cursor.execute(
        """
        SELECT
            SUM(CASE WHEN t.txn_type='Sale' THEN t.amount ELSE 0 END) AS sales,
            SUM(CASE WHEN t.txn_type IN ('Receipt', 'Sale Return') THEN t.amount ELSE 0 END) AS receipts
        FROM transactions t
        JOIN parties p ON t.party_id = p.party_id
        WHERE p.type='Credit Customer'
          AND t.txn_date >= ?
          AND t.txn_date <= ?
        """,
        (fy_start.isoformat(), fy_end.isoformat())
    )
    d_row = cursor.fetchone()
    debtors = float((d_row[0] if d_row else 0) or 0) - float((d_row[1] if d_row else 0) or 0)

    cursor.execute(
        """
        SELECT
            SUM(CASE WHEN txn_type='Sale' THEN amount WHEN txn_type IN ('Sale Return', 'Sales Return', 'Return') THEN -amount ELSE 0 END) AS sales,
            SUM(CASE WHEN txn_type='Expense' THEN amount ELSE 0 END) AS expenses
        FROM transactions
        WHERE txn_date >= ?
          AND txn_date <= ?
        """,
        (fy_start.isoformat(), fy_end.isoformat())
    )
    p_row = cursor.fetchone()
    total_sales = float((p_row[0] if p_row else 0) or 0)
    total_expenses = float((p_row[1] if p_row else 0) or 0)

    creditors = 0.0
    
    conn.close()
    
    return [
        {"account": "Cash Account", "debit": cash_bal if cash_bal > 0 else 0, "credit": -cash_bal if cash_bal < 0 else 0},
        {"account": "Bank Account", "debit": bank_bal if bank_bal > 0 else 0, "credit": -bank_bal if bank_bal < 0 else 0},
        {"account": "UPI Account", "debit": upi_bal if upi_bal > 0 else 0, "credit": -upi_bal if upi_bal < 0 else 0},
        {"account": "Sundry Debtors", "debit": debtors if debtors > 0 else 0, "credit": -debtors if debtors < 0 else 0},
        {"account": "Sundry Creditors", "debit": creditors if creditors > 0 else 0, "credit": -creditors if creditors < 0 else 0},
        {"account": "Sales Account", "debit": 0, "credit": total_sales},
        {"account": "Expense Account", "debit": total_expenses, "credit": 0}
    ]

@app.get("/report/pnl")
def get_pnl_report():
    conn = get_db_connection()
    cursor = conn.cursor()
    _, fy_start, fy_end = get_financial_year_bounds(cursor)
    cursor.execute(
        """
        SELECT
            SUM(CASE WHEN txn_type='Sale' THEN amount WHEN txn_type IN ('Sale Return', 'Sales Return', 'Return') THEN -amount ELSE 0 END) AS sales,
            SUM(CASE WHEN txn_type='Expense' THEN amount ELSE 0 END) AS expenses
        FROM transactions
        WHERE txn_date >= ?
          AND txn_date <= ?
        """,
        (fy_start.isoformat(), fy_end.isoformat())
    )
    row = cursor.fetchone()
    sales = float((row[0] if row else 0) or 0)
    expenses = float((row[1] if row else 0) or 0)
    conn.close()
    
    net_profit = sales - expenses
    
    return {
        "sales": sales,
        "expenses": expenses,
        "net_profit": net_profit
    }

@app.get("/report/dashboard")
def get_dashboard_metrics():
    conn = get_db_connection()
    cursor = conn.cursor()
    _, fy_start, fy_end = get_financial_year_bounds(cursor)

    cursor.execute(
        """
        SELECT
            SUM(CASE
                    WHEN UPPER(LTRIM(RTRIM(txn_type)))='SALE' AND txn_date = CAST(GETDATE() AS DATE) THEN amount
                    WHEN UPPER(LTRIM(RTRIM(txn_type))) IN ('SALE RETURN', 'SALES RETURN', 'RETURN') AND txn_date = CAST(GETDATE() AS DATE) THEN -amount
                    ELSE 0
                END) AS sales_today,
            SUM(CASE
                    WHEN UPPER(LTRIM(RTRIM(txn_type)))='SALE' AND MONTH(txn_date)=MONTH(GETDATE()) AND YEAR(txn_date)=YEAR(GETDATE()) THEN amount
                    WHEN UPPER(LTRIM(RTRIM(txn_type))) IN ('SALE RETURN', 'SALES RETURN', 'RETURN') AND MONTH(txn_date)=MONTH(GETDATE()) AND YEAR(txn_date)=YEAR(GETDATE()) THEN -amount
                    ELSE 0
                END) AS sales_month,
            SUM(CASE WHEN payment_mode='Cash' AND UPPER(LTRIM(RTRIM(txn_type))) IN ('SALE','RECEIPT','RECIEPT') THEN amount ELSE 0 END) AS cash_in,
            SUM(CASE WHEN payment_mode='Cash' AND UPPER(LTRIM(RTRIM(txn_type))) IN ('EXPENSE','SALE RETURN','SALES RETURN','RETURN') THEN amount ELSE 0 END) AS cash_out,
            SUM(CASE WHEN payment_mode IN ('Bank','UPI','GPay','GPAY','Google Pay','GooglePay') AND UPPER(LTRIM(RTRIM(txn_type))) IN ('SALE','RECEIPT','RECIEPT') THEN amount ELSE 0 END) AS bank_in,
            SUM(CASE WHEN payment_mode IN ('Bank','UPI','GPay','GPAY','Google Pay','GooglePay') AND UPPER(LTRIM(RTRIM(txn_type))) IN ('EXPENSE','SALE RETURN','SALES RETURN','RETURN') THEN amount ELSE 0 END) AS bank_out
        FROM transactions
        WHERE txn_date >= ?
          AND txn_date <= ?
        """,
        (fy_start.isoformat(), fy_end.isoformat())
    )
    row = cursor.fetchone()
    sales_today = float((row[0] if row else 0) or 0)
    sales_month = float((row[1] if row else 0) or 0)
    cash_in = float((row[2] if row else 0) or 0)
    cash_out = float((row[3] if row else 0) or 0)
    bank_in = float((row[4] if row else 0) or 0)
    bank_out = float((row[5] if row else 0) or 0)
    cash_bal = cash_in - cash_out
    bank_bal = bank_in - bank_out

    cursor.execute(
        """
        SELECT
            SUM(CASE WHEN UPPER(LTRIM(RTRIM(t.txn_type)))='SALE' THEN t.amount ELSE 0 END) AS cust_sales,
            SUM(CASE WHEN UPPER(LTRIM(RTRIM(t.txn_type))) IN ('RECEIPT','RECIEPT','SALE RETURN','SALES RETURN','RETURN') THEN t.amount ELSE 0 END) AS cust_receipts
        FROM transactions t
        JOIN parties p ON t.party_id=p.party_id
        WHERE p.type IN ('Customer', 'Credit Customer')
          AND t.txn_date >= ?
          AND t.txn_date <= ?
        """,
        (fy_start.isoformat(), fy_end.isoformat())
    )
    rec_row = cursor.fetchone()
    cust_sales = float((rec_row[0] if rec_row else 0) or 0)
    cust_receipts = float((rec_row[1] if rec_row else 0) or 0)
    receivables = cust_sales - cust_receipts

    conn.close()

    return {
        "sales_today": sales_today,
        "sales_month": sales_month,
        "cash_balance": cash_bal,
        "bank_balance": bank_bal,
        "receivables": receivables
    }

@app.post("/backup")
def backup_database(path: str = None, authorization: Optional[str] = Header(None)):
    require_admin(require_authenticated(authorization))
    """SQL Server backup - requires administrative privileges"""
    try:
        import datetime
        import shutil
        import os
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        database = get_runtime_database_name()
        conn = get_master_connection()
        
        # SQL Server backups cannot run in a transaction.
        conn.autocommit = True 
        
        cursor = conn.cursor()
        
        backup_dir = get_default_backup_dir()
        os.makedirs(backup_dir, exist_ok=True)
        server_backup_path = os.path.join(backup_dir, f"{database}_{timestamp}.bak")
        
        # SQL Server BACKUP command
        safe_path = escape_sql_path(server_backup_path)
        backup_query = f"BACKUP DATABASE [{escape_sql_identifier(database)}] TO DISK = '{safe_path}' WITH FORMAT, INIT"
        cursor.execute(backup_query)
        
        # CRITICAL FIX: You MUST consume the progress messages (result sets)
        # Otherwise, closing the connection aborts the backup silently!
        while cursor.nextset():
            pass
            
        conn.close()
        
        # If user provided a path, try to copy the backup there
        if path:
            try:
                shutil.copy2(server_backup_path, path)
                return {"status": "Backup Successful", "path": path}
            except Exception as copy_err:
                return {
                    "status": "Backup Successful",
                    "path": server_backup_path,
                    "warning": f"Backup saved on server at {server_backup_path}, but copy to selected path failed: {copy_err}"
                }

        return {"status": "Backup Successful", "path": server_backup_path}
    except Exception as e:
        raise HTTPException(
            status_code=500,
            detail=f"Backup failed: {e}. Ensure the SQL Server service account can write to {get_default_backup_dir()}",
        )


@app.post("/backup/auto")
def backup_database_auto(authorization: Optional[str] = Header(None)):
    require_admin(require_authenticated(authorization))
    """Automatic SQL Server backup"""
    try:
        import datetime
        import shutil
        import tempfile
        import os
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        conn = get_master_connection()
        
        conn.autocommit = True 
        
        cursor = conn.cursor()
        
        backup_dir = os.path.join(get_default_backup_dir(), "Auto")
        os.makedirs(backup_dir, exist_ok=True)

        # Permission check (Note: This checks App permissions, NOT SQL Server permissions)
        try:
            test_file = os.path.join(backup_dir, f".perm_test_{timestamp}.tmp")
            with open(test_file, "w") as f:
                f.write("test")
            os.remove(test_file)
        except Exception as perm_err:
            raise HTTPException(
                status_code=500,
                detail=f"Auto backup failed: app has no write permission on {backup_dir}. {perm_err}",
            )
        
        database = get_runtime_database_name()
        backup_path = os.path.join(backup_dir, f"auto_{database}_{timestamp}.bak")
        
        safe_path = escape_sql_path(backup_path)
        backup_query = f"BACKUP DATABASE [{escape_sql_identifier(database)}] TO DISK = '{safe_path}' WITH FORMAT, INIT"
        cursor.execute(backup_query)
        
        # CRITICAL FIX: Consume the result sets
        while cursor.nextset():
            pass
            
        conn.close()

        # Prune old backups, keep latest 10
        try:
            files = [
                os.path.join(backup_dir, f)
                for f in os.listdir(backup_dir)
                if f.lower().endswith('.bak') and os.path.basename(f).startswith('auto_')
            ]
            files.sort(key=lambda p: os.path.getmtime(p))
            while len(files) > 10:
                old = files.pop(0)
                try:
                    os.remove(old)
                except Exception:
                    pass
        except Exception:
            pass
        
        return {"status": "Backup Successful", "path": backup_path}
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Auto backup failed: {e}")

@app.post("/restore")
def restore_database(path: str, authorization: Optional[str] = Header(None)):
    require_admin(require_authenticated(authorization))
    """SQL Server restore - requires administrative privileges"""
    try:
        if not os.path.exists(path):
            raise HTTPException(status_code=404, detail="Backup file not found")
        
        database = get_runtime_database_name()
        
        conn = get_master_connection()
        cursor = conn.cursor()
        
        # Set database to single user mode before restore
        safe_database = escape_sql_identifier(database)
        cursor.execute(f"ALTER DATABASE [{safe_database}] SET SINGLE_USER WITH ROLLBACK IMMEDIATE")
        # Restore database
        safe_path = escape_sql_path(path)
        restore_query = f"RESTORE DATABASE [{safe_database}] FROM DISK = '{safe_path}' WITH REPLACE"
        cursor.execute(restore_query)
        # Set back to multi-user mode
        cursor.execute(f"ALTER DATABASE [{safe_database}] SET MULTI_USER")
        conn.close()
        
        return {"status": "Restore Successful"}
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Restore failed: {e}. SQL Server restores require admin privileges.")

from fastapi import UploadFile, File

@app.post("/import")
async def import_transactions(file: UploadFile = File(...), authorization: Optional[str] = Header(None)):
    require_authenticated(authorization)
    try:
        import shutil
        import os
        
        file_loc = f"temp_{file.filename}"
        with open(file_loc, "wb") as buffer:
            shutil.copyfileobj(file.file, buffer)
            
        import pandas as pd
        
        if file.filename.endswith('.csv'):
            df = pd.read_csv(file_loc)
        else:
            df = pd.read_excel(file_loc)
            
        os.remove(file_loc)
        
        conn = get_db_connection()
        cursor = conn.cursor()
        success = 0
        errors = 0
        failed_rows = []  # Track which rows failed and why
        party_id_cache = {}
        
        for idx, row in df.iterrows():
            row_num = idx + 2  # +2 because: 0-indexed + 1 header row
            try:
                # helper to safely get val
                def safe_get(keys, default=None):
                    for k in keys:
                        if k in row and pd.notna(row[k]):
                            return row[k]
                    return default

                date_val = safe_get(['Date', 'date', 'txn_date'])
                if not date_val:
                    failed_rows.append({"row": row_num, "reason": "Missing date"})
                    errors += 1
                    continue 
                
                # Parse Date (Handle DD.MM.YY)
                try:
                    # dayfirst=True for 17.01.26
                    dt = pd.to_datetime(date_val, dayfirst=True)
                    date = dt.strftime("%Y-%m-%d")
                except Exception as date_err:
                    failed_rows.append({"row": row_num, "reason": f"Invalid date format: {date_val}"})
                    errors += 1
                    continue

                bill = safe_get(['Bill No / Invoice', 'BillNo', 'Bill No', 'bill_no'], "")
                party = safe_get(['Customer Name', 'Party', 'party', 'Name'])
                if not party:
                    failed_rows.append({"row": row_num, "reason": "Missing party name"})
                    errors += 1
                    continue

                ttype_raw = safe_get(['Transaction Type', 'Type', 'txn_type'], "Sale")
                ttype = ttype_raw.title() if ttype_raw else "Sale" # SALE -> Sale

                mode_raw = safe_get(['Payment Mode', 'payment Mode', 'yment Mo', 'Mode', 'payment_mode'], "Cash")
                # Handle cut off header "yment Mo" just in case, though usually pandas reads full header if just invisible
                mode = mode_raw.title() if mode_raw else "Cash" # CREDIT -> Credit
                
                amt_val = safe_get(['Amount', 'amount'], 0)
                try:
                    amount = float(amt_val)
                except:
                    amount = 0.0
                
                party_key = normalize_party_key(party)
                pid = party_id_cache.get(party_key)
                if pid is None:
                    pid = get_or_create_party_id(cursor, str(party), "Customer", True)
                    party_id_cache[party_key] = pid
                txn_date = parse_date_str(date) or datetime.date.today()
                txn_fy = financial_year_for_date(txn_date)
                
                cursor.execute(
                    "INSERT INTO transactions (txn_date, bill_no, party_id, txn_type, payment_mode, financial_year, amount) VALUES (?, ?, ?, ?, ?, ?, ?)",
                    (date, bill, pid, ttype, mode, txn_fy, amount)
                )
                success += 1
            except Exception as row_err:
                failed_rows.append({"row": row_num, "reason": str(row_err)})
                errors += 1
        
        conn.close()
        invalidate_report_cache()
        
        # Build detailed response
        response_detail = f"✓ Imported: {success} rows"
        if errors > 0:
            response_detail += f" | ✗ Failed: {errors} rows"
            if len(failed_rows) <= 20:  # Show details if not too many
                response_detail += "\n\nFailed rows:\n"
                for fail in failed_rows:
                    response_detail += f"• Line {fail['row']}: {fail['reason']}\n"
            else:
                response_detail += f"\n\nShowing first 20 failed rows:\n"
                for fail in failed_rows[:20]:
                    response_detail += f"• Line {fail['row']}: {fail['reason']}\n"
                response_detail += f"... and {len(failed_rows) - 20} more"
        
        return {"status": "Imported", "detail": response_detail, "success": success, "errors": errors, "failed_rows": failed_rows}

    except ImportError:
         return {"status": "Error", "detail": "pandas/openpyxl libraries not installed."}
    except Exception as e:
        log_exception("set_opening_cash", e)
        return {"status": "Error", "detail": str(e)}

def get_opening_cash_before_date(cursor, start_date: datetime.date, opening_seed: float) -> float:
    cursor.execute(
        "SELECT TOP 1 cash_date, cash_in_hand FROM daily_cash WHERE cash_date < ? ORDER BY cash_date DESC",
        (start_date,)
    )
    row = cursor.fetchone()
    if row and row[1] is not None:
        return float(row[1])

    cursor.execute(
        """
        SELECT
            SUM(CASE WHEN payment_mode='Cash' AND UPPER(LTRIM(RTRIM(txn_type))) IN ('SALE','RECEIPT','RECIEPT') THEN amount ELSE 0 END)
            + SUM(CASE WHEN payment_mode='Credit' AND UPPER(LTRIM(RTRIM(txn_type))) IN ('RECEIPT','RECIEPT') THEN amount ELSE 0 END) AS cash_in,
            SUM(CASE WHEN payment_mode='Cash' AND UPPER(LTRIM(RTRIM(txn_type))) IN ('EXPENSE','SALE RETURN','SALES RETURN','RETURN') THEN amount ELSE 0 END) AS cash_expense
        FROM transactions
        WHERE txn_date < ?
        """,
        (start_date,)
    )
    sums = cursor.fetchone()
    cash_in = float(sums[0] or 0)
    cash_expense = float(sums[1] or 0)
    return opening_seed + cash_in - cash_expense

@app.get("/report/daily-summary")
def get_daily_summary_report(start: Optional[str] = None, end: Optional[str] = None, days: int = 30):
    start_date, end_date = resolve_date_range(start, end, days)
    cache_key = (current_company or "default", "daily_summary", str(start_date), str(end_date))
    cached = cache_get(cache_key)
    if cached is not None:
        return cached

    conn = get_db_connection()
    cursor = conn.cursor()

    cursor.execute(
        """
        SELECT DISTINCT d FROM (
            SELECT txn_date AS d FROM transactions WHERE txn_date BETWEEN ? AND ?
            UNION
            SELECT cash_date AS d FROM daily_cash WHERE cash_date BETWEEN ? AND ?
        ) dates
        """,
        (start_date, end_date, start_date, end_date)
    )
    date_rows = cursor.fetchall()
    dates = sorted([r[0] for r in date_rows])
    if not dates:
        conn.close()
        cache_set(cache_key, [])
        return []

    cursor.execute(
        """
        SELECT cash_date, cash_in_hand
        FROM daily_cash
        WHERE cash_date BETWEEN ? AND ?
        """,
        (start_date, end_date)
    )
    cash_rows = cursor.fetchall()
    cash_map = {r[0]: float(r[1]) for r in cash_rows}

    cursor.execute(
        """
        SELECT
            t.txn_date,
            SUM(CASE WHEN UPPER(LTRIM(RTRIM(t.txn_type)))='SALE' THEN t.amount
                     WHEN UPPER(LTRIM(RTRIM(t.txn_type))) IN ('SALE RETURN', 'SALES RETURN', 'RETURN') THEN -t.amount
                     ELSE 0 END) AS total_sales,
            SUM(CASE WHEN t.payment_mode='Cash' AND UPPER(LTRIM(RTRIM(t.txn_type))) IN ('SALE','RECEIPT','RECIEPT') THEN t.amount ELSE 0 END)
              + SUM(CASE WHEN t.payment_mode='Credit' AND UPPER(LTRIM(RTRIM(t.txn_type))) IN ('RECEIPT','RECIEPT') THEN t.amount ELSE 0 END) AS cash_in,
            SUM(CASE WHEN t.payment_mode='Cash' AND UPPER(LTRIM(RTRIM(t.txn_type))) IN ('EXPENSE','SALE RETURN','SALES RETURN','RETURN') THEN t.amount ELSE 0 END) AS cash_expense,
            SUM(CASE WHEN t.payment_mode IN ('Bank','UPI','GPay','GPAY','Google Pay','GooglePay') AND UPPER(LTRIM(RTRIM(t.txn_type))) IN ('SALE','RECEIPT','RECIEPT') THEN t.amount ELSE 0 END) AS bank_in,
            SUM(CASE WHEN (UPPER(LTRIM(RTRIM(t.txn_type)))='SALE' AND (p.type='Credit Customer' OR t.payment_mode='Credit')) OR UPPER(LTRIM(RTRIM(t.txn_type))) IN ('RECEIPT','RECIEPT') THEN t.amount ELSE 0 END) AS credit_sales,
            SUM(CASE WHEN UPPER(LTRIM(RTRIM(t.txn_type))) IN ('RECEIPT','RECIEPT','SALE RETURN','SALES RETURN','RETURN') AND p.type='Credit Customer' THEN t.amount ELSE 0 END) AS credit_receipts
        FROM transactions t
        LEFT JOIN parties p ON t.party_id = p.party_id
        WHERE t.txn_date BETWEEN ? AND ?
        GROUP BY t.txn_date
        """,
        (start_date, end_date)
    )
    agg_rows = cursor.fetchall()

    opening_seed = get_setting("opening_cash_seed", 0.0)
    opening_cash_seed = get_opening_cash_before_date(cursor, start_date, opening_seed) if dates else opening_seed
    conn.close()

    agg_map = {}
    for row in agg_rows:
        agg_map[row[0]] = {
            "total_sales": float(row[1] or 0),
            "cash_in": float(row[2] or 0),
            "cash_expense": float(row[3] or 0),
            "bank": float(row[4] or 0),
            "credit_sales": float(row[5] or 0),
            "credit_receipts": float(row[6] or 0)
        }

    summary = []
    prev_cash_in_hand = None
    prev_closing = opening_cash_seed

    for idx, d in enumerate(dates):
        if idx == 0:
            opening_cash = opening_cash_seed
        else:
            opening_cash = prev_cash_in_hand if prev_cash_in_hand is not None else prev_closing

        agg = agg_map.get(d, {})
        cash_in = agg.get("cash_in", 0.0)
        cash_expense = agg.get("cash_expense", 0.0)
        bank = agg.get("bank", 0.0)
        total_sales = agg.get("total_sales", 0.0)
        credit_sale = agg.get("credit_sales", 0.0) - agg.get("credit_receipts", 0.0)

        computed_closing = opening_cash + cash_in - cash_expense
        cash_in_hand = cash_map.get(d)
        cash_short_excess = 0.0
        closing_cash = computed_closing

        if cash_in_hand is not None:
            cash_short_excess = cash_in_hand - computed_closing
            closing_cash = cash_in_hand

        summary.append({
            "date": str(d),
            "opening_cash": opening_cash,
            "cash_in": cash_in,
            "cash_expense": cash_expense,
            "cash_needed": computed_closing,
            "closing_cash": closing_cash,
            "cash_in_hand": cash_in_hand,
            "cash_short_excess": cash_short_excess,
            "bank": bank,
            "credit_sale": credit_sale,
            "total_sales": total_sales
        })

        prev_cash_in_hand = cash_in_hand
        prev_closing = computed_closing

    summary = sorted(summary, key=lambda x: x["date"], reverse=True)
    cache_set(cache_key, summary)
    return summary

@app.get("/report/short-excess")
def get_short_excess_report(start: Optional[str] = None, end: Optional[str] = None, days: int = 30):
    data = get_daily_summary_report(start=start, end=end, days=days)
    return [
        {
            "date": row["date"],
            "opening_cash": row["opening_cash"],
            "cash_in": row["cash_in"],
            "cash_expense": row["cash_expense"],
            "cash_needed": row["cash_needed"],
            "cash_in_hand": row["cash_in_hand"],
            "cash_short_excess": row["cash_short_excess"]
        }
        for row in data
    ]

@app.get("/settings/opening-cash")
def get_opening_cash():
    return {"opening_cash": get_setting("opening_cash_seed", 0.0)}

@app.post("/settings/opening-cash")
def set_opening_cash(req: OpeningCashRequest, authorization: Optional[str] = Header(None)):
    require_admin(require_authenticated(authorization))
    try:
        set_setting("opening_cash_seed", float(req.amount))
        invalidate_report_cache()
        log_audit(req.admin_user, "Set Opening Cash", f"Opening Cash Seed set to {req.amount}")
        return {"status": "Saved"}
    except Exception as e:
        log_exception("set_cash_in_hand", e)
        return {"status": "Error", "detail": str(e)}

@app.post("/cash/hand")
def set_cash_in_hand(req: CashInHandRequest, authorization: Optional[str] = Header(None)):
    require_authenticated(authorization)
    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute("SELECT COUNT(*) FROM daily_cash WHERE cash_date=?", (req.date,))
        exists = cursor.fetchone()[0] > 0
        if exists:
            cursor.execute("UPDATE daily_cash SET cash_in_hand=?, updated_at=CURRENT_TIMESTAMP WHERE cash_date=?", (req.cash_in_hand, req.date))
        else:
            cursor.execute("INSERT INTO daily_cash (cash_date, cash_in_hand) VALUES (?, ?)", (req.date, req.cash_in_hand))
        conn.close()
        invalidate_report_cache()
        log_audit(req.admin_user, "Set Cash In Hand", f"{req.date} = {req.cash_in_hand}")
        return {"status": "Saved"}
    except Exception as e:
        return {"status": "Error", "detail": str(e)}

@app.post("/cash/hand/reset")
def reset_cash_in_hand(req: CashInHandResetRequest, authorization: Optional[str] = Header(None)):
    require_authenticated(authorization)
    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute("DELETE FROM daily_cash WHERE cash_date=?", (req.date,))
        conn.close()
        invalidate_report_cache()
        log_audit(req.admin_user, "Reset Cash In Hand", f"{req.date} reset to computed value")
        return {"status": "Reset"}
    except Exception as e:
        return {"status": "Error", "detail": str(e)}

# Duplicate log_audit removed


class RenamePartyRequest(BaseModel):
    old_name: str
    new_name: str
    admin_user: str

@app.post("/party/rename")
def rename_party(req: RenamePartyRequest, authorization: Optional[str] = Header(None)):
    require_admin(require_authenticated(authorization))
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        old_name = re.sub(r"\s+", " ", str(req.old_name or "").strip())
        new_name = re.sub(r"\s+", " ", str(req.new_name or "").strip())
        if not old_name or not new_name:
            raise HTTPException(status_code=400, detail="Both old and new party names are required")

        # Check if old exists
        cursor.execute("SELECT party_id FROM parties WHERE normalized_name=?", (normalize_party_key(old_name),))
        row = cursor.fetchone()
        if not row:
            # Fallback for legacy rows where normalized value may not match newer normalization.
            cursor.execute("SELECT party_id FROM parties WHERE UPPER(LTRIM(RTRIM(name))) = UPPER(LTRIM(RTRIM(?)))", (old_name,))
            row = cursor.fetchone()
        if not row:
            raise HTTPException(status_code=404, detail="Party not found")
        
        party_id = row[0]
        new_norm = normalize_party_key(new_name)
        if not new_norm:
            raise HTTPException(status_code=400, detail="Invalid new party name")
        
        # Check if new name taken
        cursor.execute("SELECT COUNT(*) FROM parties WHERE normalized_name=? AND party_id != ?", (new_norm, party_id))
        if cursor.fetchone()[0] > 0:
            raise HTTPException(status_code=400, detail="New name already exists")
            
        cursor.execute("UPDATE parties SET name=?, normalized_name=? WHERE party_id=?", (new_name, new_norm, party_id))
        conn.close()
        invalidate_report_cache()
        
        log_audit(req.admin_user, "Rename Party", f"Renamed {old_name} to {new_name}")
        return {"status": "Renamed Successfully"}
    except HTTPException:
        close_conn_safely(conn)
        raise
    except Exception as e:
        close_conn_safely(conn)
        return safe_error_response("rename_party", e, "Unable to rename party")

class CheckUserRequest(BaseModel):
    username: str

@app.post("/check-user")
def check_user(req: CheckUserRequest):
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("SELECT COUNT(*) FROM users WHERE username=?", (req.username,))
    exists = cursor.fetchone()[0] > 0
    conn.close()
    return {"exists": exists}

@app.get("/setup/status")
def get_setup_status():
    """Check if first-run admin setup is needed"""
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("SELECT password_hash FROM users WHERE username='admin'")
    row = cursor.fetchone()
    conn.close()
    
    if not row:
        return {"needs_setup": True, "reason": "Admin user not found"}
    
    # If admin has NULL password, setup is needed
    needs_setup = row[0] is None
    return {"needs_setup": needs_setup, "reason": "Admin password not configured" if needs_setup else "Setup already completed"}

@app.post("/setup/admin")
def setup_admin(req: SetupAdminRequest):
    """First-run: Set admin password (only allowed if admin has no password)"""
    if not req.password or len(req.password) < 8:
        raise HTTPException(status_code=400, detail="Password must be at least 8 characters")
    
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        # Verify admin exists and has no password
        cursor.execute("SELECT password_hash FROM users WHERE username='admin'")
        row = cursor.fetchone()
        
        if not row:
            conn.close()
            raise HTTPException(status_code=500, detail="Admin user not initialized")
        
        if row[0] is not None:
            conn.close()
            raise HTTPException(status_code=400, detail="Setup already completed. Please login instead.")
        
        # Set admin password using Argon2id
        hashed = hash_user_password(req.password)
        cursor.execute("UPDATE users SET password_hash=? WHERE username='admin'", (hashed,))
        conn.close()
        
        log_audit("system", "Setup", "Admin password set during first-run setup")
        return {"status": "Setup complete", "message": "Admin account configured. Please login."}
    except HTTPException:
        raise
    except Exception as e:
        close_conn_safely(conn)
        return safe_error_response("setup_admin", e, "Unable to complete setup")

@app.post("/login")
def login(req: LoginRequest):
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("SELECT role, password_hash FROM users WHERE username=?", (req.username,))
    row = cursor.fetchone()

    if not row:
        conn.close()
        raise HTTPException(status_code=401, detail="Invalid Credentials")

    role, stored_hash = row[0], row[1]
    is_valid, needs_upgrade = verify_user_password(stored_hash, req.password)
    if not is_valid:
        conn.close()
        raise HTTPException(status_code=401, detail="Invalid Credentials")

    # Upgrade legacy SHA-256 hash to Argon2id transparently on successful login.
    if needs_upgrade:
        upgraded_hash = hash_user_password(req.password)
        cursor.execute("UPDATE users SET password_hash=? WHERE username=?", (upgraded_hash, req.username))

    conn.close()
    token = create_access_token(req.username, role)
    log_audit(req.username, "Login", "User logged in")
    return {"status": "Success", "username": req.username, "role": role, "access_token": token, "token_type": "bearer"}

@app.get("/users")
def get_users(authorization: Optional[str] = Header(None)):
    """Get all users - admin only"""
    payload = verify_auth_header(authorization)
    require_admin(payload)
    
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("SELECT username, role FROM users")
    data = [{"username": r[0], "role": r[1]} for r in cursor.fetchall()]
    conn.close()
    return data

@app.post("/users")
def create_user(req: CreateUserRequest, authorization: Optional[str] = Header(None)):
    """Create new user - admin only"""
    payload = verify_auth_header(authorization)
    require_admin(payload)
    admin_user = payload.get("username")
    
    conn = get_db_connection()
    cursor = conn.cursor()
    # Check if exists
    cursor.execute("SELECT COUNT(*) FROM users WHERE username=?", (req.username,))
    if cursor.fetchone()[0] > 0:
        conn.close()
        raise HTTPException(status_code=400, detail="User already exists")
    
    hashed = hash_user_password(req.password)
    try:
        cursor.execute("INSERT INTO users (username, password_hash, role) VALUES (?, ?, ?)", (req.username, hashed, req.role))
        conn.close()
        log_audit(admin_user, "Create User", f"Created user {req.username} as {req.role}")
        return {"status": "User Created"}
    except Exception as e:
        close_conn_safely(conn)
        return safe_error_response("create_user", e, "Unable to create user")

@app.post("/users/password")
def change_user_password(req: ChangePasswordRequest, authorization: Optional[str] = Header(None)):
    """Change user password - admin only"""
    payload = verify_auth_header(authorization)
    require_admin(payload)
    admin_user = payload.get("username")
    
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        cursor.execute("SELECT COUNT(*) FROM users WHERE username=?", (req.username,))
        if cursor.fetchone()[0] == 0:
            conn.close()
            raise HTTPException(status_code=404, detail="User not found")

        hashed = hash_user_password(req.new_password)
        cursor.execute("UPDATE users SET password_hash=? WHERE username=?", (hashed, req.username))
        conn.close()

        log_audit(admin_user, "Change Password", f"Password changed for {req.username}")
        return {"status": "Password Updated"}
    except Exception as e:
        close_conn_safely(conn)
        return safe_error_response("change_user_password", e, "Unable to change password")

@app.delete("/users/{username}")
def delete_user(username: str, authorization: Optional[str] = Header(None)):
    """Delete user - admin only"""
    payload = verify_auth_header(authorization)
    require_admin(payload)
    admin_user = payload.get("username")
    
    conn = get_db_connection()
    cursor = conn.cursor()
    if username == "admin":
        conn.close()
        raise HTTPException(status_code=400, detail="Cannot delete default admin")
    
    cursor.execute("DELETE FROM users WHERE username=?", (username,))
    conn.close()
    log_audit(admin_user, "Delete User", f"Deleted user {username}")
    return {"status": "User Deleted"}

@app.get("/audit")
def get_audit_logs(authorization: Optional[str] = Header(None)):
    """Get audit logs - admin only"""
    payload = verify_auth_header(authorization)
    require_admin(payload)
    
    conn = get_db_connection()
    cursor = conn.cursor()
    comp = current_company or "default"
    # Show all audit logs for the current company
    cursor.execute("SELECT timestamp, username, action, details FROM audit_logs WHERE company=? ORDER BY timestamp DESC", (comp,))
    data = [{"timestamp": str(r[0]), "username": r[1], "action": r[2], "details": r[3]} for r in cursor.fetchall()]
    conn.close()
    return data

# Transaction Editing (Admin Only)
class EditTxnRequest(BaseModel):
    txn_id: int
    field: str
    new_value: str

@app.post("/transaction/edit")
def edit_transaction(req: EditTxnRequest, authorization: Optional[str] = Header(None)):
    """Edit transaction - admin only"""
    payload = verify_auth_header(authorization)
    require_admin(payload)
    admin_user = payload.get("username")
    
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        # Static map prevents brittle SQL interpolation if request field names drift.
        field_map = {
            "txn_date": "txn_date",
            "bill_no": "bill_no",
            "party": "party_id",
            "txn_type": "txn_type",
            "payment_mode": "payment_mode",
            "amount": "amount",
        }
        db_field = field_map.get(req.field)
        if not db_field:
            raise HTTPException(status_code=400, detail="Invalid field")

        # Get old value for audit
        if req.field == "party":
            cursor.execute(
                """
                SELECT p.name
                FROM transactions t
                INNER JOIN parties p ON p.party_id = t.party_id
                WHERE t.txn_id=?
                """,
                (req.txn_id,)
            )
        else:
            cursor.execute(f"SELECT {db_field} FROM transactions WHERE txn_id=?", (req.txn_id,))
        row = cursor.fetchone()
        if not row:
            raise HTTPException(status_code=404, detail="Transaction not found")
        
        old_val = str(row[0])

        if req.field == "party":
            normalized = normalize_party_key(req.new_value)
            if not normalized:
                raise HTTPException(status_code=400, detail="Party name is required")

            cursor.execute("SELECT party_id FROM parties WHERE normalized_name=?", (normalized,))
            party_row = cursor.fetchone()
            if not party_row:
                raise HTTPException(status_code=404, detail="Party not found")
            cursor.execute("UPDATE transactions SET party_id=? WHERE txn_id=?", (party_row[0], req.txn_id))
        elif req.field == "txn_date":
            txn_date = parse_date_str(req.new_value)
            if not txn_date:
                raise HTTPException(status_code=400, detail="Invalid transaction date")
            cursor.execute(
                "UPDATE transactions SET txn_date=?, financial_year=? WHERE txn_id=?",
                (txn_date.isoformat(), financial_year_for_date(txn_date), req.txn_id)
            )
        else:
            cursor.execute(f"UPDATE transactions SET {db_field}=? WHERE txn_id=?", (req.new_value, req.txn_id))

        conn.close()
        invalidate_report_cache()
        
        audit_field = "party" if req.field == "party" else db_field
        log_audit(admin_user, "Edit Transaction", f"Changed {audit_field} from {old_val} to {req.new_value} for Txn ID {req.txn_id}")
        return {"status": "Updated Successfully"}
    except HTTPException:
        close_conn_safely(conn)
        raise
    except Exception as e:
        close_conn_safely(conn)
        return safe_error_response("edit_transaction", e, "Unable to update transaction")

class DeleteTxnRequest(BaseModel):
    txn_id: int

@app.post("/transaction/delete")
def delete_transaction(req: DeleteTxnRequest, authorization: Optional[str] = Header(None)):
    """Delete transaction - admin only"""
    payload = verify_auth_header(authorization)
    require_admin(payload)
    admin_user = payload.get("username")
    
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        # Fetch details for audit
        cursor.execute("SELECT txn_date, bill_no, amount, party_id FROM transactions WHERE txn_id=?", (req.txn_id,))
        row = cursor.fetchone()
        if not row:
            raise HTTPException(status_code=404, detail="Transaction not found")
            
        details = f"Date: {row[0]}, Bill: {row[1]}, Amount: {row[2]}"
        
        cursor.execute("DELETE FROM transactions WHERE txn_id=?", (req.txn_id,))
        conn.close()
        invalidate_report_cache()
        
        log_audit(admin_user, "Delete Transaction", f"Deleted Txn ID {req.txn_id}. {details}")
        return {"status": "Deleted Successfully"}
    except Exception as e:
        close_conn_safely(conn)
        return safe_error_response("delete_transaction", e, "Unable to delete transaction")

# Database Configuration Unlock (No Auth Required - used before login)
class DbConfigUnlockRequest(BaseModel):
    password: str

class DbConfigUnlockSetupRequest(BaseModel):
    password: str

def _get_app_setting(cursor, key: str, default_val: Optional[str] = None) -> Optional[str]:
    cursor.execute("SELECT setting_value FROM app_settings WHERE setting_key=?", (key,))
    row = cursor.fetchone()
    if not row:
        return default_val
    value = row[0]
    return default_val if value is None else str(value)

def _set_app_setting(cursor, key: str, value: str):
    cursor.execute("SELECT COUNT(*) FROM app_settings WHERE setting_key=?", (key,))
    exists = cursor.fetchone()[0] > 0
    if exists:
        cursor.execute("UPDATE app_settings SET setting_value=? WHERE setting_key=?", (value, key))
    else:
        cursor.execute("INSERT INTO app_settings (setting_key, setting_value) VALUES (?, ?)", (key, value))

@app.get("/dbconfig/unlock-status")
def get_dbconfig_unlock_status():
    """DB config locking is disabled; config is always accessible."""
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        cursor.execute(
            "DELETE FROM app_settings WHERE setting_key IN (?, ?, ?)",
            (
                DBCONFIG_UNLOCK_PASSWORD_KEY,
                DBCONFIG_UNLOCK_FAIL_COUNT_KEY,
                DBCONFIG_UNLOCK_LOCKED_UNTIL_KEY,
            ),
        )
        conn.close()
    except Exception:
        close_conn_safely(conn)
    return {"needs_setup": False, "disabled": True}

@app.post("/dbconfig/unlock")
def verify_dbconfig_unlock(req: DbConfigUnlockRequest):
    """DB config locking is disabled; accept any request and clear legacy lock state."""
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        cursor.execute(
            "DELETE FROM app_settings WHERE setting_key IN (?, ?, ?)",
            (
                DBCONFIG_UNLOCK_PASSWORD_KEY,
                DBCONFIG_UNLOCK_FAIL_COUNT_KEY,
                DBCONFIG_UNLOCK_LOCKED_UNTIL_KEY,
            ),
        )
        conn.close()
        return {"status": "Success", "message": "Unlocked", "disabled": True}
    except Exception as e:
        close_conn_safely(conn)
        log_exception("verify_dbconfig_unlock", e)
        return {"status": "Success", "message": "Unlocked", "disabled": True}

@app.post("/dbconfig/unlock/setup")
def setup_dbconfig_unlock(req: DbConfigUnlockSetupRequest):
    """DB config locking is disabled; clear any legacy state and skip setup."""
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        cursor.execute(
            "DELETE FROM app_settings WHERE setting_key IN (?, ?, ?)",
            (
                DBCONFIG_UNLOCK_PASSWORD_KEY,
                DBCONFIG_UNLOCK_FAIL_COUNT_KEY,
                DBCONFIG_UNLOCK_LOCKED_UNTIL_KEY,
            ),
        )
        conn.close()
        return {"status": "Setup complete", "disabled": True}
    except Exception as e:
        close_conn_safely(conn)
        log_exception("setup_dbconfig_unlock", e)
        return {"status": "Setup complete", "disabled": True}

# Database Configuration Endpoints
class DbConfigRequest(BaseModel):
    server: str
    database: str
    auth_type: str  # 'windows' or 'sql'
    username: str = ""
    password: str = ""
    backup_dir: str = ""

@app.get("/config/database")
def get_db_config(authorization: Optional[str] = Header(None)):
    """Get current database configuration - admin only"""
    payload = verify_auth_header(authorization)
    require_admin(payload)
    
    cfg = load_runtime_config()
    return {
        "server": cfg["server"],
        "database": cfg["database"],
        "auth_type": cfg["auth_type"],
        "username": cfg["username"],
        "backup_dir": cfg["backup_dir"]
    }

@app.post("/config/database/test")
def test_db_config(req: DbConfigRequest, authorization: Optional[str] = Header(None)):
    """Test database connection - admin only"""
    payload = verify_auth_header(authorization)
    require_admin(payload)
    
    try:
        if req.auth_type == "windows":
            conn_str = f"DRIVER={{ODBC Driver 17 for SQL Server}};SERVER={req.server};DATABASE={req.database};Trusted_Connection=yes;TrustServerCertificate=yes;"
        else:
            conn_str = f"DRIVER={{ODBC Driver 17 for SQL Server}};SERVER={req.server};DATABASE={req.database};UID={req.username};PWD={req.password};TrustServerCertificate=yes;"
        
        # Test connection
        test_conn = pyodbc.connect(conn_str, timeout=5, autocommit=True)
        test_conn.close()
        
        return {"success": True, "message": "Connection successful"}
    except Exception as e:
        return {"success": False, "error": str(e)}

@app.post("/config/database")
def save_db_config(req: DbConfigRequest, authorization: Optional[str] = Header(None)):
    """Save database configuration - admin only"""
    payload = verify_auth_header(authorization)
    require_admin(payload)
    
    try:
        import json
        
        config_data = {
            "server": req.server,
            "database": req.database,
            "auth_type": req.auth_type,
            "username": req.username if req.auth_type == "sql" else "",
            "password": req.password if req.auth_type == "sql" else "",
            "backup_dir": req.backup_dir
        }
        
        # Save to config.py
        auth_comment = 'SQL Server' if req.auth_type == 'sql' else 'Windows'
        safe_server = json.dumps(req.server or "")
        safe_database = json.dumps(req.database or "")
        safe_username = json.dumps(req.username if req.auth_type == "sql" else "")
        safe_password = json.dumps(req.password if req.auth_type == "sql" else "")
        safe_backup_dir = json.dumps(req.backup_dir or "")
        config_content = f"""# SQL Server Configuration
# Update these values with your SQL Server connection details

# Common values:
# - "localhost" or "(local)" for default instance
# - "localhost\\SQLEXPRESS" for SQL Express
# - "localhost\\MSSQLSERVER" for named instance
# - "." for local default instance
SQL_SERVER = {safe_server}  # Your SQL Server instance

SQL_DATABASE = {safe_database}  # database name
SQL_USERNAME = {safe_username}  # SQL Server username (leave empty for Windows Auth)
SQL_PASSWORD = {safe_password}  # SQL Server password (leave empty for Windows Auth)
SQL_DRIVER = "{{ODBC Driver 17 for SQL Server}}"  # or "{{SQL Server}}" for older versions
BACKUP_TARGET_DIR = {safe_backup_dir}  # UNC share path for SQL Server backups (optional)

# Connection string - {auth_comment} Authentication
"""
        if req.auth_type == "windows":
            config_content += f'CONNECTION_STRING = f"DRIVER={{SQL_DRIVER}};SERVER={{SQL_SERVER}};DATABASE={{SQL_DATABASE}};Trusted_Connection=yes;TrustServerCertificate=yes;"\n'
        else:
            config_content += f'CONNECTION_STRING = f"DRIVER={{SQL_DRIVER}};SERVER={{SQL_SERVER}};DATABASE={{SQL_DATABASE}};UID={{SQL_USERNAME}};PWD={{SQL_PASSWORD}};TrustServerCertificate=yes;"\n'
        
        try:
            with open("config.py", "w") as f:
                f.write(config_content)
        except Exception:
            pass
        
        # Also save to JSON for easy retrieval
        os.makedirs(CONFIG_DIR, exist_ok=True)
        with open(CONFIG_FILE, "w") as f:
            json.dump(config_data, f, indent=2)
        
        return {"success": True, "message": "Configuration saved. Please restart the application."}
    except Exception as e:
        return {"success": False, "error": str(e)}

