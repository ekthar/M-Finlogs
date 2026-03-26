import json
import urllib.request
import urllib.error

payload = {
    'financial_year': '2025-2026',
    'month': 'March',
    'rows': [
        {'name': 'Payar', 'qty': [25, 22, 20]},
        {'name': 'Vendakka', 'qty': [0, 0, 0]}
    ]
}

req = urllib.request.Request(
    'http://127.0.0.1:8000/report/inventory/pdf-preview',
    data=json.dumps(payload).encode('utf-8'),
    headers={'Content-Type': 'application/json'}
)

try:
    r = urllib.request.urlopen(req)
    print('status', r.status)
    print('content-type', r.getheader('Content-Type'))
except urllib.error.HTTPError as e:
    print('status', e.code)
    print(e.read().decode('utf-8', 'ignore'))
except Exception as e:
    print(type(e).__name__, str(e))
