M-Finlogs Server Package
========================

Files:
- backend.exe
- db_config.json

Run on server:
1. Install Microsoft ODBC Driver for SQL Server.
2. Edit db_config.json for your SQL server settings if needed.
3. Run backend.exe
4. Keep port 8000 open in Windows Firewall.

Note:
- backend.exe uses JWT secret from environment variable JWT_SECRET_KEY when provided.
- If JWT secret is not provided, ensure your deployment process sets one.
