var electronAPI = window.electronAPI;
var ExcelJS = null;
var XLSX = window.XLSX || null;

// Global cache for party types
const partyTypes = {};
let salesChartInstance = null;
let expenseChartInstance = null;
let currentRole = null; // Global variable for user role
let backendStatusIntervalId = null;
let uiFailsafeIntervalId = null;
let appInitDone = false;
let isSavingEntry = false;
let isLoggingIn = false;

const DEFAULT_API_BASE = 'http://127.0.0.1:8000';
let apiBase = DEFAULT_API_BASE;

function normalizeApiBase(url) {
    const trimmed = (url || '').trim();
    if (!trimmed) return DEFAULT_API_BASE;
    const withProto = /^https?:\/\//i.test(trimmed) ? trimmed : `http://${trimmed}`;
    return withProto.replace(/\/$/, '');
}

async function loadClientConfig() {
    try {
        return await electronAPI.invoke('config:readClient');
    } catch (e) {
        return {};
    }
}

async function saveClientConfig(configJson) {
    const res = await electronAPI.invoke('config:writeClient', configJson);
    if (!res || !res.success) {
        throw new Error((res && res.error) || 'Failed to write client config');
    }
}

async function initApiBase() {
    const cfg = await loadClientConfig();
    if (cfg.api_base) {
        apiBase = normalizeApiBase(cfg.api_base);
    }
}

const originalFetch = window.fetch.bind(window);
window.fetch = (url, options) => {
    if (typeof url === 'string' && url.startsWith(DEFAULT_API_BASE)) {
        url = url.replace(DEFAULT_API_BASE, apiBase);
    }
    return originalFetch(url, options);
};

// Database Configuration Functions - defined early for inline handlers
function showDbConfig() {
    const accessModal = document.getElementById('dbConfigAccessModal');
    if (!accessModal) {
        showToast('Configuration access modal not found', 'error');
        return;
    }
    
    // First, check if DB config unlock password needs to be set up
    fetch('http://127.0.0.1:8000/dbconfig/unlock-status')
        .then(r => r.json())
        .then(data => {
            if (data.needs_setup) {
                // First-time setup: show setup dialog
                showDbConfigSetupModal();
            } else {
                // Password already set: show unlock dialog
                showDbConfigAccessModal();
            }
        })
        .catch(e => {
            // If backend unavailable, just show access modal
            showDbConfigAccessModal();
        });
}

function showDbConfigAccessModal() {
    const accessModal = document.getElementById('dbConfigAccessModal');
    const passEl = document.getElementById('dbConfigAccessPass');
    const errEl = document.getElementById('dbConfigAccessError');
    if (!accessModal || !passEl) {
        showToast('Configuration access modal not found', 'error');
        return;
    }
    if (errEl) errEl.textContent = '';
    passEl.value = '';
    setModalVisible('dbConfigAccessModal', true);
    passEl.style.display = 'block';
    setTimeout(() => passEl.focus(), 0);
}

function showDbConfigSetupModal() {
    const accessModal = document.getElementById('dbConfigAccessModal');
    const passEl = document.getElementById('dbConfigAccessPass');
    const errEl = document.getElementById('dbConfigAccessError');
    const titleEl = accessModal ? accessModal.querySelector('.login-subtitle') : null;
    const submitBtn = accessModal ? accessModal.querySelector('button.login-btn:not(.login-btn--ghost)') : null;
    
    if (!accessModal || !passEl) {
        showToast('Configuration modal not found', 'error');
        return;
    }
    
    // Set up for first-time password creation
    if (titleEl) titleEl.textContent = 'Create DB Config Unlock Password';
    if (submitBtn) submitBtn.textContent = 'Create Password';
    if (errEl) errEl.textContent = '';
    passEl.value = '';
    passEl.placeholder = 'Create unlock password (min 6 chars)';
    passEl.dataset.issetup = 'true';
    setModalVisible('dbConfigAccessModal', true);
    setTimeout(() => passEl.focus(), 0);
}

function closeDbConfigAccessModal() {
    const accessModal = document.getElementById('dbConfigAccessModal');
    const passEl = document.getElementById('dbConfigAccessPass');
    const titleEl = accessModal ? accessModal.querySelector('.login-subtitle') : null;
    const submitBtn = accessModal ? accessModal.querySelector('button.login-btn:not(.login-btn--ghost)') : null;
    
    setModalVisible('dbConfigAccessModal', false);
    
    // Reset modal to default state
    if (titleEl) titleEl.textContent = 'Enter password to open Configure Database';
    if (submitBtn) submitBtn.textContent = 'Unlock';
    if (passEl) {
        passEl.dataset.issetup = 'false';
        passEl.placeholder = 'Enter password';
    }
}

function submitDbConfigAccess() {
    const passEl = document.getElementById('dbConfigAccessPass');
    const errEl = document.getElementById('dbConfigAccessError');
    const dbModal = document.getElementById('dbConfigModal');
    const isSetup = passEl?.dataset.issetup === 'true';
    
    if (!passEl || !dbModal) return;

    if (!passEl.value.trim()) {
        if (errEl) errEl.textContent = isSetup ? 'Enter a password' : 'Incorrect password';
        showToast(isSetup ? 'Enter a password' : 'Incorrect password', 'error');
        passEl.focus();
        passEl.select();
        return;
    }

    if (isSetup) {
        // First-time setup: create the unlock password (pre-login allowed)

        fetch('http://127.0.0.1:8000/dbconfig/unlock/setup', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ password: passEl.value })
        })
        .then(r => r.json())
        .then(data => {
            if (data.status === 'Setup complete') {
                showToast('DB Config unlock password created!', 'success');
                closeDbConfigAccessModal();
                setModalVisible('dbConfigModal', true);
                loadDbConfig();
            } else {
                if (errEl) errEl.textContent = data.detail || 'Setup failed';
                showToast(data.detail || 'Setup failed', 'error');
            }
        })
        .catch(e => {
            if (errEl) errEl.textContent = 'Error: ' + e.message;
            showToast('Error: ' + e.message, 'error');
        });
    } else {
        // Password verification: unlock DB config
        fetch('http://127.0.0.1:8000/dbconfig/unlock', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ password: passEl.value })
        })
        .then(r => r.json())
        .then(data => {
            if (data.status === 'Success') {
                closeDbConfigAccessModal();
                setModalVisible('dbConfigModal', true);
                loadDbConfig();
            } else {
                if (errEl) errEl.textContent = 'Incorrect password';
                showToast('Incorrect password', 'error');
                passEl.focus();
                passEl.select();
            }
        })
        .catch(e => {
            if (errEl) errEl.textContent = 'Error: ' + e.message;
            showToast('Error: ' + e.message, 'error');
        });
    }
}

function closeDbConfig() {
    setModalVisible('dbConfigModal', false);
}

function toggleSqlAuth() {
    const authType = document.getElementById('cfgAuthType').value;
    const sqlFields = document.getElementById('sqlAuthFields');
    if (sqlFields) {
        sqlFields.style.display = authType === 'sql' ? 'block' : 'none';
    }
}

// Declare testDbConnection and saveDbConfig as window functions (will be defined later)
// This allows inline handlers to work
window.showDbConfig = showDbConfig;
window.submitDbConfigAccess = submitDbConfigAccess;
window.closeDbConfigAccessModal = closeDbConfigAccessModal;
window.closeDbConfig = closeDbConfig;
window.toggleSqlAuth = toggleSqlAuth;
// testDbConnection and saveDbConfig are assigned at the bottom of the file
window.installServerMode = installServerMode;
window.uninstallServerMode = uninstallServerMode;
window.restartBackend = restartBackend;
window.stopBackend = stopBackend;
window.runAutoBackupNow = runAutoBackupNow;
window.openAutoBackupFolder = openAutoBackupFolder;
window.requestUpdateCheck = requestUpdateCheck;
window.requestUpdateRestart = requestUpdateRestart;

const PARTY_CACHE_TTL_MS = 5 * 60 * 1000;
let partyCache = { data: null, ts: 0 };
const REPORT_TIMEOUT_MS = 20000;

function setModalVisible(modalId, visible, displayMode = 'flex') {
    const modal = document.getElementById(modalId);
    if (!modal) return;
    if (!modal.dataset.defaultZIndex) {
        modal.dataset.defaultZIndex = modal.style.zIndex || window.getComputedStyle(modal).zIndex || '';
    }
    if (visible) {
        modal.style.display = displayMode;
        modal.style.pointerEvents = 'auto';
        modal.style.visibility = 'visible';
        modal.style.zIndex = modal.dataset.defaultZIndex || '';
    } else {
        modal.style.display = 'none';
        modal.style.pointerEvents = 'none';
        modal.style.visibility = 'hidden';
        modal.style.zIndex = modal.dataset.defaultZIndex || '';
    }
}

// Centralized fetch wrapper with timeout and error handling
async function fetchWithTimeout(url, options = {}, timeout = 5000, allowStatuses = []) {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), timeout);
    try {
        const response = await fetch(url, { ...options, signal: controller.signal });
        clearTimeout(timeoutId);
        if (!response.ok && !allowStatuses.includes(response.status)) {
            let errorMessage = `HTTP ${response.status}: ${response.statusText}`;
            try {
                const contentType = (response.headers.get('content-type') || '').toLowerCase();
                if (contentType.includes('application/json')) {
                    const payload = await response.clone().json();
                    const detail = payload?.detail || payload?.message;
                    if (detail) {
                        errorMessage = String(detail);
                    }
                } else {
                    const text = (await response.clone().text() || '').trim();
                    if (text) {
                        errorMessage = text.length > 300 ? text.slice(0, 300) : text;
                    }
                }
            } catch (_ignored) {
                // keep default message
            }
            const err = new Error(errorMessage);
            err.status = response.status;
            throw err;
        }
        return response;
    } catch (error) {
        clearTimeout(timeoutId);
        if (error.name === 'AbortError') {
            throw new Error('Request timeout - server not responding');
        }
        throw error;
    }
}

function fetchReport(url) {
    return fetchWithTimeout(url, {}, REPORT_TIMEOUT_MS);
}

function toYMD(dateObj) {
    const y = dateObj.getFullYear();
    const m = String(dateObj.getMonth() + 1).padStart(2, '0');
    const d = String(dateObj.getDate()).padStart(2, '0');
    return `${y}-${m}-${d}`;
}

function initializeDatePickers() {
    if (typeof flatpickr !== 'function') return;
    const nodes = document.querySelectorAll('input.app-date-input');
    nodes.forEach((el) => {
        if (el._flatpickr) {
            el._flatpickr.destroy();
        }
        flatpickr(el, {
            dateFormat: 'Y-m-d',
            allowInput: true,
            disableMobile: true,
            clickOpens: true,
            static: false,
            onReady: (_selectedDates, _dateStr, instance) => {
                const cal = instance.calendarContainer;
                if (!cal || cal.querySelector('.app-fp-footer')) return;

                const footer = document.createElement('div');
                footer.className = 'app-fp-footer';

                const todayBtn = document.createElement('button');
                todayBtn.type = 'button';
                todayBtn.className = 'app-fp-today-btn';
                todayBtn.textContent = 'Today';
                todayBtn.addEventListener('click', () => {
                    instance.setDate(new Date(), true);
                    instance.close();
                });

                footer.appendChild(todayBtn);
                cal.appendChild(footer);
            }
        });
    });
}

async function loadParties(force = false) {
    try {
        const now = Date.now();
        let data = partyCache.data;

        if (!force && data && (now - partyCache.ts) < PARTY_CACHE_TTL_MS) {
            // Use cached parties
        } else {
            const res = await fetchWithTimeout("http://127.0.0.1:8000/parties");
            data = await res.json();
            partyCache = { data, ts: now };
        }

        const datalist = document.getElementById("partyList");
        const reportDrop = document.getElementById("reportParty");

        Object.keys(partyTypes).forEach((key) => delete partyTypes[key]);
        
        // Build HTML strings first
        let datalistHTML = "";
        let reportDropHTML = '<option value="">Select Party</option>';

        data.forEach(p => {
            partyTypes[p.name] = p.type;
            datalistHTML += `<option value="${p.name}">`;
            reportDropHTML += `<option value="${p.name}">${p.name}</option>`;
        });
        
        // Set innerHTML once
        datalist.innerHTML = datalistHTML;
        if (reportDrop && reportDrop.tagName === 'SELECT') {
            reportDrop.innerHTML = reportDropHTML;
        }
    } catch (e) {
        showToast("Error loading parties: " + e, "error");
    }
}

let currentTxnPage = 1;
let totalTxnPages = 1;
let appStartTime = Date.now();
let currentTransactionsCache = [];
let txnRangeDays = 30; // default to last 30 days for faster initial load
const TXN_CACHE_TTL_MS = 60 * 1000;
let txnPageCache = { key: null, ts: 0, data: null };
let allTransactions = [];
const TXN_ROW_HEIGHT = 36;
const TXN_OVERSCAN = 8;
const TXN_STATIC_RENDER_THRESHOLD = 200;
let txnVirtualInitialized = false;
const TXN_HIGHLIGHT_TTL_MS = 24 * 60 * 60 * 1000;
let txnHighlightMap = null;
let editContext = null;
// Guard flag to prevent concurrent loadTransactions calls
// Multiple rapid calls cause DOM thrashing -> input fields freeze
// Fixed: Only one loadTransactions can run at a time
let isLoadingTransactions = false;
let isCompactTables = false;
let outstandingCallListMode = false;
let softScrollInitialized = false;
let txnMeasuredRowHeight = TXN_ROW_HEIGHT;
let dayBookMeasuredRowHeight = 36;
let auditMeasuredRowHeight = 32;
let txnRenderVersion = 0;
let txnLastRenderKey = '';
let dayBookRenderVersion = 0;
let dayBookLastRenderKey = '';

function getMeasuredRowHeight(tbody, spacerClass, fallback) {
    if (!tbody) return fallback;
    const row = tbody.querySelector(`tr:not(.${spacerClass})`);
    if (!row) return fallback;
    const height = Math.round(row.getBoundingClientRect().height);
    return Number.isFinite(height) && height > 8 ? height : fallback;
}

function resetVirtualRowHeights() {
    txnMeasuredRowHeight = TXN_ROW_HEIGHT;
    dayBookMeasuredRowHeight = DAYBOOK_ROW_HEIGHT;
    auditMeasuredRowHeight = AUDIT_ROW_HEIGHT;
}

function initSoftScroll() {
    if (softScrollInitialized) return;
    softScrollInitialized = true;

    // Keep native wheel behavior for virtualized tables; custom wheel animation
    // causes jitter when scroll position drives virtual rendering.
    const selectors = ['.main-content'];
    const states = new WeakMap();

    const attach = (container) => {
        if (!container || container.dataset.softScrollBound === '1') return;
        container.dataset.softScrollBound = '1';
        container.addEventListener('wheel', (e) => {
            if (e.target && e.target.closest('.txn-table-container, .daybook-table-container, .audit-table-container')) return;
            if (e.ctrlKey || e.metaKey) return;
            if (Math.abs(e.deltaY) < 1) return;
            if (container.scrollHeight <= container.clientHeight) return;

            e.preventDefault();

            const unit = e.deltaMode === 1 ? 32 : (e.deltaMode === 2 ? container.clientHeight * 0.9 : 1);
            const delta = e.deltaY * unit;
            const max = Math.max(0, container.scrollHeight - container.clientHeight);

            const state = states.get(container) || { target: container.scrollTop, raf: 0 };
            state.target = Math.min(max, Math.max(0, state.target + delta));

            const animate = () => {
                const dist = state.target - container.scrollTop;
                if (Math.abs(dist) < 0.7) {
                    container.scrollTop = state.target;
                    state.raf = 0;
                    states.set(container, state);
                    return;
                }
                container.scrollTop += dist * 0.18;
                state.raf = requestAnimationFrame(animate);
                states.set(container, state);
            };

            if (!state.raf) {
                state.raf = requestAnimationFrame(animate);
            }
            states.set(container, state);
        }, { passive: false });
    };

    selectors.forEach((selector) => {
        document.querySelectorAll(selector).forEach(attach);
    });
}

function initTxnVirtualScroll() {
    if (txnVirtualInitialized) return;
    const container = document.getElementById('txnTableContainer');
    if (!container) return;
    txnVirtualInitialized = true;
    let scrollIdleTimer = null;
    let ticking = false;
    container.addEventListener('scroll', () => {
        container.classList.add('is-scrolling');
        if (scrollIdleTimer) clearTimeout(scrollIdleTimer);
        scrollIdleTimer = setTimeout(() => {
            container.classList.remove('is-scrolling');
        }, 90);
        if (ticking) return;
        ticking = true;
        requestAnimationFrame(() => {
            renderVirtualizedTransactions();
            ticking = false;
        });
    });
}

function loadTxnHighlights() {
    if (txnHighlightMap) return txnHighlightMap;
    try {
        txnHighlightMap = JSON.parse(localStorage.getItem('txnHighlights') || '{}') || {};
    } catch (e) {
        txnHighlightMap = {};
    }
    return txnHighlightMap;
}

function saveTxnHighlights() {
    if (!txnHighlightMap) return;
    localStorage.setItem('txnHighlights', JSON.stringify(txnHighlightMap));
}

function pruneTxnHighlights() {
    const map = loadTxnHighlights();
    const now = Date.now();
    let changed = false;
    Object.keys(map).forEach((key) => {
        if (!map[key] || (now - map[key].ts) > TXN_HIGHLIGHT_TTL_MS) {
            delete map[key];
            changed = true;
        }
    });
    if (changed) saveTxnHighlights();
}

function markTxnHighlight(id, type) {
    if (!id) return;
    const map = loadTxnHighlights();
    map[String(id)] = { type, ts: Date.now() };
    saveTxnHighlights();
}

function getTxnHighlightClass(id) {
    if (!id) return '';
    const map = loadTxnHighlights();
    const entry = map[String(id)];
    if (!entry) return '';
    if ((Date.now() - entry.ts) > TXN_HIGHLIGHT_TTL_MS) {
        delete map[String(id)];
        saveTxnHighlights();
        return '';
    }
    return entry.type === 'edited' ? 'row-edited' : entry.type === 'deleted' ? 'row-deleted' : '';
}

function captureEditContext() {
    const activeView = document.querySelector('.view-section.active');
    const viewId = activeView ? activeView.id : 'entryView';
    const txnContainer = document.getElementById('txnTableContainer');
    const dayBookContainer = document.getElementById('dayBookTableContainer');
    editContext = {
        viewId,
        txnScrollTop: txnContainer ? txnContainer.scrollTop : 0,
        dayBookScrollTop: dayBookContainer ? dayBookContainer.scrollTop : 0
    };
}

function restoreEditContext() {
    if (!editContext) return;
    const { viewId, txnScrollTop, dayBookScrollTop } = editContext;
    editContext = null;

    if (viewId === 'entryView') {
        const container = document.getElementById('txnTableContainer');
        if (container) container.scrollTop = txnScrollTop;
        return;
    }

    if (viewId === 'dayBookView') {
        showView('dayBookView', { skipLoad: true });
        const nav = document.querySelector('a[onclick*="showDayBook"]');
        if (nav) activateNav(nav);
        refreshDayBookIfNeeded();
        const container = document.getElementById('dayBookTableContainer');
        if (container) container.scrollTop = dayBookScrollTop;
        return;
    }

    if (viewId === 'ledgerView') {
        showView('ledgerView', { skipLoad: true });
        const nav = document.querySelector('a[onclick*="ledgerView"]');
        if (nav) activateNav(nav);
        loadLedgerReport();
        return;
    }

    showView(viewId, { skipLoad: true });
    const nav = document.querySelector(`a[onclick*="${viewId}"]`);
    if (nav) activateNav(nav);
}

function renderVirtualizedTransactions(resetScroll = false) {
    const container = document.getElementById('txnTableContainer');
    const tbody = document.getElementById('transactionList');
    if (!container || !tbody) return;

    if (resetScroll) {
        container.scrollTop = 0;
    }

    const data = allTransactions || [];
    const total = data.length;
    const containerHeight = container.clientHeight || 520;
    const scrollTop = container.scrollTop || 0;

    const measured = getMeasuredRowHeight(tbody, 'txn-spacer', txnMeasuredRowHeight);
    txnMeasuredRowHeight = measured || txnMeasuredRowHeight;
    const rowHeight = txnMeasuredRowHeight;
    const useStaticRender = total <= TXN_STATIC_RENDER_THRESHOLD;
    const start = useStaticRender ? 0 : Math.max(0, Math.floor(scrollTop / rowHeight) - TXN_OVERSCAN);
    const visibleCount = useStaticRender ? total : Math.ceil(containerHeight / rowHeight) + TXN_OVERSCAN * 2;
    const end = Math.min(total, start + visibleCount);
    const renderKey = `${txnRenderVersion}:${currentRole || ''}:${start}:${end}:${total}`;
    if (!resetScroll && renderKey === txnLastRenderKey) {
        return;
    }
    txnLastRenderKey = renderKey;

    const topPad = useStaticRender ? 0 : start * rowHeight;
    const bottomPad = useStaticRender ? 0 : Math.max(0, (total - end) * rowHeight);
    const colspan = currentRole === 'admin' ? 7 : 6;

    let rowsHTML = '';
    if (topPad > 0) {
        rowsHTML += `<tr class="txn-spacer"><td colspan="${colspan}" style="height:${topPad}px; border:none; padding:0;"></td></tr>`;
    }

    pruneTxnHighlights();
    for (let i = start; i < end; i += 1) {
        const txn = data[i];
        const rowClass = getTxnHighlightClass(txn.id);
        const safeDate = escapeHtml(formatDateShort(txn.date));
        const safeBillNo = escapeHtml(txn.bill_no || '');
        const safeParty = escapeHtml(txn.party || '');
        const safeType = escapeHtml(txn.type || '');
        const safeMode = escapeHtml(txn.mode || '');
        const safeAmount = escapeHtml(formatMoney(txn.amount));
        let actionCell = "";
        if (currentRole === 'admin') {
            actionCell = `<td class="action-cell">
                <button class="btn-action edit" onclick="openEditModal('${txn.id}')" title="Edit">
                    <span class="material-symbols-rounded" aria-hidden="true">edit</span>
                    <span>Edit</span>
                </button>
                <button class="btn-action delete" onclick="if(!window.isDeleting) deleteTransaction('${txn.id}')" title="Delete">
                    <span class="material-symbols-rounded" aria-hidden="true">delete</span>
                    <span>Delete</span>
                </button>
            </td>`;
        }

        rowsHTML += `
        <tr${rowClass ? ` class="${rowClass}"` : ''}>
            <td>${safeDate}</td>
            <td>${safeBillNo}</td>
            <td>${safeParty}</td>
            <td>${safeType}</td>
            <td>${safeMode}</td>
            <td>${safeAmount}</td>
            ${actionCell}
        </tr>`;
    }

    if (bottomPad > 0) {
        rowsHTML += `<tr class="txn-spacer"><td colspan="${colspan}" style="height:${bottomPad}px; border:none; padding:0;"></td></tr>`;
    }

    tbody.innerHTML = rowsHTML;
}

function invalidateTxnCache() {
    txnPageCache = { key: null, ts: 0, data: null };
}

function updateTxnRangeUI() {
    const btn = document.getElementById('txnRangeToggleBtn');
    if (!btn) return;
    if (txnRangeDays && txnRangeDays > 0) {
        btn.textContent = 'Last 30 Days';
    } else {
        btn.textContent = 'All Transactions';
    }
}

function toggleTxnRange() {
    txnRangeDays = (txnRangeDays && txnRangeDays > 0) ? 0 : 30;
    txnPageCache = { key: null, ts: 0, data: null };
    updateTxnRangeUI();
    loadTransactions(1);
}

async function loadTransactions(page = 1, force = false, options = {}) {
    const { resetScroll = true } = options;
    if (isLoadingTransactions) {
        return;
    }
    
    isLoadingTransactions = true;
    
    try {
        if (page < 1 || (page > totalTxnPages && totalTxnPages > 0)) {
            isLoadingTransactions = false;
            return;
        }
        
        const rangeParam = (txnRangeDays && txnRangeDays > 0) ? `&days=${txnRangeDays}` : '';
        const cacheKey = `page=${page}&limit=50${rangeParam}`;
        let response = null;

        if (!force && page === 1 && txnPageCache.key === cacheKey && (Date.now() - txnPageCache.ts) < TXN_CACHE_TTL_MS) {
            response = txnPageCache.data;
        } else {
            const res = await fetchWithTimeout(`http://127.0.0.1:8000/transactions?${cacheKey}`);
            response = await res.json();
            if (page === 1) {
                txnPageCache = { key: cacheKey, ts: Date.now(), data: response };
            }
        }
        
        // Validate response format
        if (!response || !response.transactions) {
            showToast("Error: Invalid server response", "error");
            return;
        }
        
        const data = response.transactions;
        currentTransactionsCache = data;
        currentTxnPage = response.page || 1;
        totalTxnPages = response.total_pages || 1;

        // Ensure latest-first: date desc, then bill desc
        const sortedData = [...data].sort((a, b) => {
            const da = new Date(a.date || 0).getTime();
            const db = new Date(b.date || 0).getTime();
            if (da !== db) return db - da;
            const ba = getBillSortValue(a.bill_no);
            const bb = getBillSortValue(b.bill_no);
            if (ba !== bb) return bb - ba;
            return String(b.bill_no || '').localeCompare(String(a.bill_no || ''));
        });

        // Admin action column header (Daily Transactions table)
        const headerRow = document.querySelector('#entryTable thead tr');
        if (headerRow) {
            const existHeader = headerRow.querySelector('.action-header');
            if (existHeader) existHeader.remove();

            if (currentRole === 'admin') {
                const th = document.createElement('th');
                th.className = 'action-header';
                th.innerText = 'Action';
                headerRow.appendChild(th);
            }
        }

        allTransactions = sortedData;
        txnRenderVersion += 1;
        txnLastRenderKey = '';
        initTxnVirtualScroll();
        const container = document.getElementById('txnTableContainer');
        const prevScrollTop = container ? container.scrollTop : 0;
        renderVirtualizedTransactions(resetScroll);
        if (!resetScroll && container) {
            container.scrollTop = prevScrollTop;
        }

        // Set default date if not set
        const dateInput = document.getElementById("newDate");
        if (!dateInput.value) {
            dateInput.value = toYMD(new Date());
        }
        
        // Update pagination controls
        const pageInfo = document.getElementById('txnPageInfo');
        const currentPageDisplay = document.getElementById('currentPageDisplay');
        const prevBtn = document.getElementById('prevPageBtn');
        const nextBtn = document.getElementById('nextPageBtn');

        updateTxnRangeUI();
        
        if (pageInfo) {
            const rangeLabel = (txnRangeDays && txnRangeDays > 0) ? `Last ${txnRangeDays} days` : 'All time';
            pageInfo.textContent = `(${response.total} entries • ${rangeLabel})`;
        }
        if (currentPageDisplay) currentPageDisplay.textContent = `Page ${currentTxnPage} of ${totalTxnPages}`;
        if (prevBtn) prevBtn.disabled = currentTxnPage <= 1;
        if (nextBtn) nextBtn.disabled = currentTxnPage >= totalTxnPages;
        
        // Force ensure app is interactive after loading - AGGRESSIVE
        const appContent = document.getElementById('appContent');
        if (appContent) {
            appContent.style.setProperty('pointer-events', 'auto', 'important');
            appContent.style.setProperty('filter', 'none', 'important');
            appContent.style.setProperty('opacity', '1', 'important');
        }
    } catch (e) {
        showToast("Error loading transactions: " + e, "error");
    } finally {
        isLoadingTransactions = false;
    }
}

function incrementBillNumber(billNo) {
    if (!billNo || billNo.trim() === "") return "";
    
    // Match pattern: prefix (letters/symbols) + number
    // Examples: L-145, INV-99, BILL123, A1, 145
    const match = billNo.match(/^(.*?)(\d+)$/);
    
    if (!match) {
        // No number found, return as is
        return billNo;
    }
    
    const prefix = match[1]; // Everything before the number (e.g., "L-", "INV-", "BILL", "A")
    const numberPart = match[2]; // The numeric part (e.g., "145", "99")
    const numberLength = numberPart.length; // Preserve leading zeros
    
    // Increment the number
    const incremented = (parseInt(numberPart, 10) + 1).toString();
    
    // Pad with zeros if original had leading zeros
    const paddedNumber = incremented.padStart(numberLength, '0');
    
    return prefix + paddedNumber;
}

function getBillSortValue(billNo) {
    const str = (billNo || '').toString().trim();
    const match = str.match(/(\d+)(?!.*\d)/);
    return match ? parseInt(match[1], 10) : Number.MAX_SAFE_INTEGER;
}

async function saveEntry() {
    if (isSavingEntry) return;
    isSavingEntry = true;

    const date = document.getElementById("newDate").value;
    const bill = document.getElementById("newBill").value;
    const partyInput = document.getElementById("newParty");
    const party = partyInput.value.trim();
    const type = document.getElementById("newType").value;
    const mode = document.getElementById("newMode").value;
    const amount = document.getElementById("newAmount").value;
    const isStrict = document.getElementById("strictPartyMode").checked;
        // Validate amount
        if (!amount || isNaN(amount) || parseFloat(amount) <= 0) {
            showToast("Amount must be greater than 0", "error");
            document.getElementById("newAmount").focus();
            isSavingEntry = false;
            return;
        }
    
    // Check if we're in edit mode
    if (editingTransactionId) {
        await updateTransaction(editingTransactionId, date, bill, party, type, mode, amount);
        isSavingEntry = false;
        return;
    }

    if (!date || !amount || !type || !mode || !party) {
        showToast("All fields (Date, Party, Type, Mode, Amount) are mandatory.", "error");
        isSavingEntry = false;
        return;
    }

    if (mode === "Credit") {
        const pType = partyTypes[party];
        if (pType && pType !== "Credit Customer") {
            showToast(`Credit Mode is NOT allowed for '${party}' (Type: ${pType}). Only 'Credit Customer' can take credit.`, "error");
            isSavingEntry = false;
            return;
        }
    }

    if (!party && mode === "Credit") {
        showToast("Credit transactions require a party.", "error");
        isSavingEntry = false;
        return;
    }

    const datalist = document.getElementById("partyList");
    let partyExists = false;
    for (let i = 0; i < datalist.options.length; i++) {
        if (datalist.options[i].value === party) {
            partyExists = true;
            break;
        }
    }

    if (!partyExists && party !== "") {
        if (isStrict) {
            showToast(`Party '${party}' not found. Create it in 'Manage > Add Party' first.`, "error");
            isSavingEntry = false;
            return;
        } else {
            if (confirm(`Party '${party}' does not exist. Create new Party?`)) {
                let pType = prompt("Enter Party Type (Customer, Credit Customer, Supplier, Expense Account, Bank):", "Customer");
                if (!pType) {
                    isSavingEntry = false;
                    return;
                }

                const isCredit = pType === "Credit Customer";
                await fetchWithTimeout("http://127.0.0.1:8000/party", {
                    method: "POST",
                    headers: { "Content-Type": "application/json" },
                    body: JSON.stringify({ name: party, ptype: pType, credit: isCredit })
                });
                await loadParties(true);
            } else {
                isSavingEntry = false;
                return;
            }
        }
    }

    const partyParam = party ? party.replace(/ /g, "%20") : "customer";

    // Construct URL for POST
    const url = `http://127.0.0.1:8000/transaction?date=${date}&bill_no=${bill}&party=${partyParam}&txn_type=${type}&mode=${mode}&amount=${amount}`;

    try {
        const res = await fetchWithTimeout(url, { method: "POST" });
        const data = await res.json();

        const statusText = (data.status || "").toString().trim().toLowerCase();
        if (statusText.includes("added") || statusText.includes("saved") || statusText.includes("success")) {
            showToast("Entry Saved Successfully!", "success");
            invalidateTxnCache();
            await loadTransactions(currentTxnPage, true);
            
            // Auto-increment bill number
            const currentBill = bill;
            const nextBill = incrementBillNumber(currentBill);
            
            // Clear inputs
            document.getElementById("newBill").value = nextBill;
            document.getElementById("newParty").value = "Customer";
            document.getElementById("newAmount").value = "";
            document.getElementById("newType").value = "Sale";
            document.getElementById("newMode").value = "Credit";
            document.getElementById("newBill").focus();
        } else {
            showToast("Error: " + JSON.stringify(data), "error");
        }
    } catch (e) {
        showToast("Network Error: " + e, "error");
    } finally {
        isSavingEntry = false;
    }
}

function handleEntryNavigation(event, nextFieldIdOrAction) {
    if (event.key === "Enter") {
        event.preventDefault();
        
        if (nextFieldIdOrAction === 'save') {
            // Save and focus will be handled by saveEntry
            saveEntry();
        } else {
            // Navigate to next field
            const nextField = document.getElementById(nextFieldIdOrAction);
            if (nextField) {
                nextField.focus();
                // If it's a select, open the dropdown
                if (nextField.tagName === 'SELECT') {
                    nextField.click();
                }
            }
        }
    }
}

async function createParty() {
    const name = document.getElementById("partyName").value;
    const type = document.getElementById("partyType").value;
    const credit = document.getElementById("creditAllowed").checked;

    if (!name) return showToast("Enter Party Name", "error");

    try {
        const res = await fetchWithTimeout("http://127.0.0.1:8000/party", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ name, ptype: type, credit })
        });
        const data = await res.json();
        if (data.status === "Party Created") {
            showToast("Party Created Successfully", "success");
            document.getElementById("partyName").value = "";
            loadParties(true);
        } else {
            showToast("Error: " + data.detail, "error");
        }
    } catch (e) {
        showToast("Error: " + e, "error");
    }
}

// View Switching
function showView(viewId, options = {}) {
    const { skipLoad = false } = options;
    document.querySelectorAll('.view-section').forEach(el => el.classList.remove('active'));
    document.getElementById(viewId).classList.add('active');
    updateGlobalReportContext(viewId);
    
    // Load data when switching to specific views
    if (skipLoad) {
        return;
    }

    if (viewId === 'entryView') {
        loadTransactions();
    } else if (viewId === 'ledgerView') {
        loadLedgerReport();
    } else if (viewId === 'dayBookView') {
        loadDayBook();
    } else if (viewId === 'inventoryView') {
        loadInventoryMonth();
    } else if (viewId === 'inventoryValueView') {
        loadInventoryValueMonth();
    } else if (viewId === 'auditView') {
        loadAuditLog('auditTableBodyView');
    }
}

function toggleTableDensity() {
    isCompactTables = !isCompactTables;
    document.body.classList.toggle('compact-tables', isCompactTables);
    const btn = document.getElementById('densityToggleBtn');
    if (btn) btn.textContent = isCompactTables ? 'Comfortable View' : 'Compact View';
    resetVirtualRowHeights();
}

function applySidebarCollapsed(collapsed) {
    const sidebar = document.querySelector('.sidebar');
    const toggleBtn = document.getElementById('sidebarToggleBtn');
    if (!sidebar) return;

    sidebar.classList.toggle('collapsed', !!collapsed);
    localStorage.setItem('sidebarCollapsed', collapsed ? '1' : '0');

    if (toggleBtn) {
        const icon = toggleBtn.querySelector('ion-icon');
        if (icon) {
            icon.setAttribute('name', collapsed ? 'chevron-forward-outline' : 'chevron-back-outline');
        }
        toggleBtn.title = collapsed ? 'Expand sidebar' : 'Minimize sidebar';
        toggleBtn.setAttribute('aria-label', toggleBtn.title);
    }
}

function toggleSidebar() {
    const sidebar = document.querySelector('.sidebar');
    if (!sidebar) return;
    applySidebarCollapsed(!sidebar.classList.contains('collapsed'));
}

function updateGlobalReportContext(viewId) {
    const el = document.getElementById('globalReportContext');
    if (!el) return;

    const fy = sessionStorage.getItem('financialYear') || '-';
    let range = 'FY default';

    if (viewId === 'ledgerView') {
        const s = document.getElementById('ledgerStart')?.value;
        const e = document.getElementById('ledgerEnd')?.value;
        range = s || e ? `${s || '-'} to ${e || '-'}` : 'Selected FY';
    } else if (viewId === 'purchaseReportView') {
        const s = document.getElementById('purchaseFrom')?.value;
        const e = document.getElementById('purchaseTo')?.value;
        range = s || e ? `${s || '-'} to ${e || '-'}` : 'Selected FY';
    } else if (viewId === 'dailySummaryView') {
        const s = document.getElementById('dailyFrom')?.value;
        const e = document.getElementById('dailyTo')?.value;
        range = s || e ? `${s || '-'} to ${e || '-'}` : 'Selected FY';
    } else if (viewId === 'shortReportView') {
        const s = document.getElementById('shortFrom')?.value;
        const e = document.getElementById('shortTo')?.value;
        range = s || e ? `${s || '-'} to ${e || '-'}` : 'Selected FY';
    } else if (viewId === 'inventoryView') {
        const monthSelect = document.getElementById('inventoryMonthSelect');
        const label = monthSelect && monthSelect.options.length
            ? monthSelect.options[monthSelect.selectedIndex].text
            : 'Selected month';
        range = label;
    } else if (viewId === 'inventoryValueView') {
        const monthSelect = document.getElementById('inventoryValueMonthSelect');
        const label = monthSelect && monthSelect.options.length
            ? monthSelect.options[monthSelect.selectedIndex].text
            : 'Selected month';
        range = label;
    }

    el.textContent = `FY: ${fy} | Range: ${range}`;
}

// Reports
// Reports
async function loadLedgerReport() {
    const party = document.getElementById("reportParty").value;
    if (!party) return showToast("Select a party first", "error");

    const normalizedParty = party.toLowerCase().replace(/\s+/g, "_");

    // Get Dates
    const start = document.getElementById("ledgerStart").value;
    const end = document.getElementById("ledgerEnd").value;

    let url = `http://127.0.0.1:8000/ledger/${encodeURIComponent(normalizedParty)}`;
    const params = new URLSearchParams();
    if (start) params.append("start", start);
    if (end) params.append("end", end);
    if (start || end) url += `?${params.toString()}`;

    const res = await fetchWithTimeout(url);
    const payload = await res.json();
    const data = Array.isArray(payload) ? payload : (payload.data || []);

    const tbody = document.getElementById("ledgerBody");
    tbody.innerHTML = "";

    const openingChip = document.getElementById('ledgerOpeningChip');
    if (openingChip) {
        let openingBalance = Number(payload && payload.opening_balance != null ? payload.opening_balance : 0);
        if (Array.isArray(payload) && data.length > 0) {
            const first = data[0];
            const firstType = String(first.type || '').toLowerCase();
            const firstAmt = Number(first.amount || 0);
            let effect = 0;
            if (firstType === 'sale') effect = firstAmt;
            else if (firstType === 'receipt' || firstType === 'reciept' || firstType === 'sale return') effect = -firstAmt;
            openingBalance = Number(first.balance || 0) - effect;
        }
        const openingLabel = openingBalance < 0
            ? `${formatMoney(Math.abs(openingBalance))} Cr`
            : `${formatMoney(openingBalance)} Dr`;
        const startLabel = payload && payload.period_start ? formatDateShort(payload.period_start) : 'selected period';
        openingChip.textContent = `Opening (${startLabel}): ${openingLabel}`;
        openingChip.classList.remove('positive', 'negative');
        openingChip.classList.add(openingBalance < 0 ? 'negative' : 'positive');
        openingChip.style.display = 'inline-flex';
    }

    // Admin Edit Column Header
    const headerRow = document.querySelector('#ledgerTable thead tr');
    // Remove if exists to avoid duplicates
    const existHeader = document.getElementById('editHeader');
    if (existHeader) existHeader.remove();

    if (currentRole === 'admin') {
        const th = document.createElement('th');
        th.id = 'editHeader';
        th.innerText = 'Action';
        // Append to end
        headerRow.appendChild(th);
    }

    pruneTxnHighlights();
    let rowsHtml = '';
    data.forEach(row => {
        const rowClass = getTxnHighlightClass(row.id);
        let actionCell = "";
        if (currentRole === 'admin') {
            actionCell = `<td class="action-cell">
                <button class="btn-action edit" onclick="openEditModal('${row.id}')" title="Edit">
                    <span class="material-symbols-rounded" aria-hidden="true">edit</span>
                    <span>Edit</span>
                </button>
            </td>`;
        }

        const typeRaw = (row.type || '').toString().trim();
        const type = typeRaw.toLowerCase();
        const mode = row.mode ? row.mode : '';
        const particulars = `${typeRaw}${mode ? ' / ' + mode : ''}`;
        const amt = Number(row.amount || 0);

        // Use backend's calculated balance
        const serverBalance = Number(row.balance || 0);

        // Determine debit/credit display based on transaction type for receivable ledger
        // Sale = Debit (increases receivable)
        // Receipt, Sale Return = Credit (decreases receivable)
        let debit = '';
        let credit = '';
        if (type === 'sale') {
            debit = formatMoney(amt);
        } else if (type === 'receipt' || type === 'reciept' || type === 'sale return') {
            credit = formatMoney(amt);
        } else {
            // Other types: show in debit but don't affect receivable balance
            debit = formatMoney(amt);
        }

        const balanceLabel = serverBalance < 0
            ? `${formatMoney(Math.abs(serverBalance))} Cr`
            : `${formatMoney(serverBalance)} Dr`;

        rowsHtml += `
        <tr${rowClass ? ` class="${rowClass}"` : ''}>
            <td>${formatDateShort(row.date)}</td>
            <td>${row.bill_no || ''}</td>
            <td>${particulars}</td>
            <td class="text-right">${debit}</td>
            <td class="text-right">${credit}</td>
            <td class="text-right">${balanceLabel}</td>
            ${actionCell}
        </tr>`;
    });
    tbody.innerHTML = rowsHtml || `<tr><td colspan="6" class="text-right">No Data</td></tr>`;
}

// --- Edit Transaction Logic ---
let editingTransactionId = null;

async function openEditModal(id) {
    try {
        captureEditContext();
        // Try cached transactions first
        let txn = currentTransactionsCache.find(t => t.id == id);

        // Fallback: use efficient single transaction endpoint
        if (!txn) {
            const res = await fetchWithTimeout(`http://127.0.0.1:8000/transaction/${id}`);
            txn = await res.json();
        }
        
        if (!txn) {
            showToast('Transaction not found', 'error');
            return;
        }
        
        // Set edit mode
        editingTransactionId = id;
        
        // Switch to entry view
        showView('entryView', { skipLoad: true });
        activateNav(document.querySelector('a[onclick*="entryView"]'));
        
        // Pre-fill the form with transaction data
        document.getElementById('newDate').value = txn.date;
        document.getElementById('newBill').value = txn.bill_no || '';
        document.getElementById('newParty').value = txn.party;
        document.getElementById('newType').value = txn.type;
        document.getElementById('newMode').value = txn.mode;
        document.getElementById('newAmount').value = txn.amount;
        
        // Show edit indicator banner with visual feedback
        let editBanner = document.getElementById('editModeBanner');
        if (!editBanner) {
            editBanner = document.createElement('div');
            editBanner.id = 'editModeBanner';
            editBanner.style.cssText = 'background: linear-gradient(135deg, #7C3AED, #A855F7); color: white; padding: 12px 16px; margin: 12px 0; border-radius: 8px; display: flex; justify-content: space-between; align-items: center; font-weight: 500; box-shadow: 0 4px 12px rgba(124, 58, 237, 0.3);';
            
            const textSpan = document.createElement('span');
            textSpan.id = 'editBannerText';
            textSpan.innerHTML = `✏️ Editing Transaction #${id} - Press Ctrl+S to Save or Enter`;
            editBanner.appendChild(textSpan);
            
            const cancelBtn = document.createElement('button');
            cancelBtn.textContent = '✖ Cancel Edit';
            cancelBtn.style.cssText = 'background: rgba(255,255,255,0.2); border: 1px solid rgba(255,255,255,0.3); color: white; padding: 6px 12px; border-radius: 6px; cursor: pointer; font-weight: 500;';
            cancelBtn.onclick = cancelEdit;
            editBanner.appendChild(cancelBtn);
            
            const entryCard = document.querySelector('#entryView .card');
            if (entryCard && entryCard.parentNode) {
                entryCard.parentNode.insertBefore(editBanner, entryCard);
            }
        } else {
            document.getElementById('editBannerText').innerHTML = `✏️ Editing Transaction #${id} - Press Ctrl+S to Save or Enter`;
        }
        editBanner.style.display = 'flex';
        
        // Focus on the first field
        document.getElementById('newDate').focus();
        
        showToast(`📝 Editing #${id} - Ctrl+S to Save or press Enter in Amount`, 'info');
    } catch (e) {
        showToast('Error loading transaction: ' + e, 'error');
    }
}

function resetEntryEditState(shouldNotify) {
    editingTransactionId = null;
    
    // Reset form
    document.getElementById('newBill').value = '';
    document.getElementById('newParty').value = 'Customer';
    document.getElementById('newAmount').value = '';
    document.getElementById('newType').value = 'Sale';
    document.getElementById('newMode').value = 'Credit';
    
    // Hide edit banner
    const editBanner = document.getElementById('editModeBanner');
    if (editBanner) editBanner.style.display = 'none';
    
    if (shouldNotify) {
        showToast('Edit cancelled', 'info');
    }
}

function cancelEdit() {
    resetEntryEditState(true);
}

async function updateTransaction(id, date, bill, party, type, mode, amount) {
    try {
        // Update each field that changed
        const updates = [
            { field: 'party', value: party },
            { field: 'txn_date', value: date },
            { field: 'bill_no', value: bill },
            { field: 'txn_type', value: type },
            { field: 'payment_mode', value: mode },
            { field: 'amount', value: amount }
        ];
        
        for (const update of updates) {
            const token = sessionStorage.getItem('access_token');
            const res = await fetch("http://127.0.0.1:8000/transaction/edit", {
                method: "POST",
                headers: {
                    "Content-Type": "application/json",
                    ...(token ? { 'Authorization': `Bearer ${token}` } : {})
                },
                body: JSON.stringify({
                    txn_id: parseInt(id),
                    field: update.field,
                    new_value: update.value
                })
            });
            
            const data = await res.json();
            if (data.status !== "Updated Successfully") {
                showToast(`Update failed for ${update.field}: ` + data.detail, "error");
                return;
            }
        }
        
        showToast("Transaction Updated Successfully!", "success");
        markTxnHighlight(id, 'edited');
        
        // Reset edit mode
        cancelEdit();
        
        // Reload transactions
        invalidateTxnCache();
        await loadTransactions(currentTxnPage, true, { resetScroll: false });
        updateDashboard();
        refreshDayBookIfNeeded();
        restoreEditContext();
        
    } catch (e) {
        showToast("Error updating transaction: " + e, "error");
    }
}

function closeEditModal() {
    setModalVisible('editTxnModal', false);
    const appContent = document.getElementById('appContent');
    if (appContent) {
        appContent.style.pointerEvents = 'auto';
        appContent.style.filter = 'none';
        appContent.style.opacity = '1';
    }
}

function ensureAppInteractive() {
    const appContent = document.getElementById('appContent');
    const loginModal = document.getElementById('loginModal');
    const editModal = document.getElementById('editTxnModal');
    const confirmModal = document.getElementById('confirmDeleteModal');
    const dbModal = document.getElementById('dbConfigModal');

    if (sessionStorage.getItem('username')) {
        if (loginModal) {
            loginModal.style.display = 'none';
            loginModal.style.pointerEvents = 'none';
        }
        if (appContent) {
            appContent.style.setProperty('filter', 'none', 'important');
            appContent.style.setProperty('pointer-events', 'auto', 'important');
            appContent.style.setProperty('opacity', '1', 'important');
        }
    }

    // Aggressively hide all modals
    if (editModal) {
        editModal.style.display = 'none';
        editModal.style.pointerEvents = 'none';
        editModal.style.visibility = 'hidden';
    }
    if (confirmModal) {
        confirmModal.style.display = 'none';
        confirmModal.style.pointerEvents = 'none';
    }
    if (dbModal) {
        dbModal.style.display = 'none';
        dbModal.style.pointerEvents = 'none';
    }
    
    // CRITICAL: Reset all input fields to ensure they're interactive
    const inputFields = ['newDate', 'newBill', 'newParty', 'newType', 'newMode', 'newAmount', 'newRemarks'];
    inputFields.forEach(id => {
        const input = document.getElementById(id);
        if (input) {
            input.disabled = false;
            input.readOnly = false;
            input.style.pointerEvents = 'auto';
            input.style.opacity = '1';
            input.style.filter = 'none';
        }
    });
}

async function submitTxnEdit() {
    const id = document.getElementById('editTxnId').value;
    const field = document.getElementById('editField').value;
    const val = document.getElementById('editValue').value;

    if (!val) return showToast("Enter a value", "error");

    try {
        const token = sessionStorage.getItem('access_token');
        const res = await fetch("http://127.0.0.1:8000/transaction/edit", {
            method: "POST",
            headers: {
                "Content-Type": "application/json",
                ...(token ? { 'Authorization': `Bearer ${token}` } : {})
            },
            body: JSON.stringify({
                txn_id: parseInt(id),
                field: field,
                new_value: val
            })
        });

        const data = await res.json();
        if (data.status === "Updated Successfully") {
            showToast("Transaction Updated", "success");
            markTxnHighlight(id, 'edited');
            closeEditModal();
            ensureAppInteractive();
            
            // Small delay to let DOM settle before reload
            setTimeout(() => {
                if (isViewActive('ledgerView')) loadLedgerReport();
                invalidateTxnCache();
                loadTransactions(currentTxnPage, true, { resetScroll: false });
                updateDashboard();
                refreshDayBookIfNeeded();
                restoreEditContext();
            }, 50);
        } else {
            showToast("Update Failed: " + data.detail, "error");
        }
    } catch (e) {
        showToast("Error: " + e, "error");
    }
}

function showConfirmDelete(id) {
    const okBtn = document.getElementById('confirmOkBtn');
    const cancelBtn = document.getElementById('confirmCancelBtn');
    
    pendingDeleteId = id;
    setModalVisible('confirmDeleteModal', true);

    if (okBtn) okBtn.onclick = () => {
        setModalVisible('confirmDeleteModal', false);
        const deleteId = pendingDeleteId;
        pendingDeleteId = null;
        performDelete(deleteId);
    };
    
    if (cancelBtn) cancelBtn.onclick = () => {
        setModalVisible('confirmDeleteModal', false);
        pendingDeleteId = null;
        unlockUiAfterModal();
    };
}

async function deleteTransaction(id) {
    // Prevent concurrent deletes
    if (isDeleting) {
        return;
    }

    if (!id) {
        showToast('Delete failed: missing transaction id', 'error');
        return;
    }

    captureEditContext();
    
    // Close edit modal and show confirmation
    closeEditModal();
    showConfirmDelete(id);
}

async function performDelete(id) {
    setDeleteBusy(true);
    let shouldReactivateInputs = false;
    
    try {
        if (!id) {
            throw new Error('Delete failed: missing transaction id');
        }

        const token = sessionStorage.getItem('access_token');
        const res = await fetchWithTimeout("http://127.0.0.1:8000/transaction/delete", {
            method: "POST",
            headers: {
                "Content-Type": "application/json",
                ...(token ? { 'Authorization': `Bearer ${token}` } : {})
            },
            body: JSON.stringify({
                txn_id: parseInt(id)
            })
        });

        const data = await res.json();
        if (data.status === "Deleted Successfully") {
            showToast("Transaction Deleted", "success");
            markTxnHighlight(id, 'deleted');
            if (String(editingTransactionId) === String(id)) {
                resetEntryEditState(false);
            }
            
            // Reload data
            if (isViewActive('ledgerView')) loadLedgerReport();
            invalidateTxnCache();
            await loadTransactions(currentTxnPage, true, { resetScroll: false });
            updateDashboard();
            refreshDayBookIfNeeded();
            restoreEditContext();
            shouldReactivateInputs = true;
        } else {
            showToast("Delete Failed: " + data.detail, "error");
        }
    } catch (e) {
        showToast("Error: " + e, "error");
    } finally {
        pendingDeleteId = null;
        unlockUiAfterModal();
        setDeleteBusy(false);
        if (shouldReactivateInputs) {
            setTimeout(() => {
                reactivateInputs();
            }, 100);
        }
    }
}

function unlockUiAfterModal() {
    const appContent = document.getElementById('appContent');
    const loginModal = document.getElementById('loginModal');
    const editModal = document.getElementById('editTxnModal');
    const confirmModal = document.getElementById('confirmDeleteModal');
    const dbModal = document.getElementById('dbConfigModal');

    const rootEl = document.documentElement;
    const bodyEl = document.body;

    [rootEl, bodyEl, appContent].forEach(el => {
        if (!el) return;
        el.style.pointerEvents = 'auto';
        el.style.filter = 'none';
        el.style.opacity = '1';
    });

    [loginModal, editModal, confirmModal, dbModal].forEach(modal => {
        if (!modal) return;
        modal.style.display = 'none';
        modal.style.pointerEvents = 'none';
        modal.style.visibility = 'hidden';
        if (modal.dataset.defaultZIndex) {
            modal.style.zIndex = modal.dataset.defaultZIndex;
        }
    });

    if (document.activeElement && document.activeElement !== document.body) {
        document.activeElement.blur();
    }
}

function reactivateInputs() {
    unlockUiAfterModal();
    
    // 1. Force parent container reset
    const cardContainer = document.querySelector('#entryView .card');
    if (cardContainer) {
        const originalDisplay = cardContainer.style.display;
        cardContainer.style.display = 'none';
        void cardContainer.offsetHeight;
        cardContainer.style.display = originalDisplay || '';
    }
    
    // 2. Reset all input fields completely
    const inputFields = ['newDate', 'newBill', 'newParty', 'newType', 'newMode', 'newAmount'];
    inputFields.forEach(id => {
        const input = document.getElementById(id);
        if (input) {
            // Clear all potentially blocking properties
            input.disabled = false;
            input.readOnly = false;
            input.removeAttribute('disabled');
            input.removeAttribute('readonly');
            input.style.cssText = '';
            
            // Force visual reset
            input.style.pointerEvents = 'auto';
            input.style.opacity = '1';
            input.style.cursor = 'text';
            
            // Trigger browser re-registration
            input.blur();
            input.dispatchEvent(new Event('change'));
        }
    });
    
    // 3. Ensure app container is interactive
    const appContent = document.getElementById('appContent');
    if (appContent) {
        appContent.style.setProperty('pointer-events', 'auto', 'important');
        appContent.style.setProperty('filter', 'none', 'important');
        appContent.style.setProperty('opacity', '1', 'important');
        void appContent.offsetHeight;
    }
    
    // 4. Focus first input to verify it works
    setTimeout(() => {
        const firstInput = document.getElementById('newBill');
        if (firstInput) {
            firstInput.focus();
        }
    }, 50);
}

// Charts Logic
function renderCharts(data) {
    const salesCanvas = document.getElementById('salesChart');
    const expenseCanvas = document.getElementById('expenseChart');
    if (!salesCanvas || !expenseCanvas) return;

    const ctxSales = salesCanvas.getContext('2d');
    const ctxExps = expenseCanvas.getContext('2d');

    Chart.defaults.font.family = 'Google Sans, Inter, sans-serif';
    Chart.defaults.font.size = 12;
    Chart.defaults.color = '#6B7280';

    if (salesChartInstance) {
        salesChartInstance.data.datasets[0].data = [data.sales_today, data.sales_month];
        salesChartInstance.update();
    } else {
        const salesGradient = ctxSales.createLinearGradient(0, 0, 0, 400);
        salesGradient.addColorStop(0, '#4F46E5'); // Deep Indigo
        salesGradient.addColorStop(1, '#818CF8'); // Light Indigo

        salesChartInstance = new Chart(ctxSales, {
            type: 'bar',
            data: {
                labels: ['Today', 'This Month'],
                datasets: [{
                    label: 'Sales',
                    data: [data.sales_today, data.sales_month],
                    backgroundColor: salesGradient,
                    borderRadius: 8,
                    barPercentage: 0.6
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    y: { beginAtZero: true, grid: { color: '#F3F4F6' } },
                    x: { grid: { display: false } }
                },
                plugins: { legend: { display: false } }
            }
        });
    }

    if (expenseChartInstance) {
        expenseChartInstance.data.datasets[0].data = [data.cash_balance, data.bank_balance];
        expenseChartInstance.update();
    } else {
        const expensesGradient = ctxExps.createLinearGradient(0, 0, 0, 400);
        expensesGradient.addColorStop(0, '#10B981'); // Emerald
        expensesGradient.addColorStop(1, '#34D399');

        expenseChartInstance = new Chart(ctxExps, {
            type: 'doughnut',
            data: {
                labels: ['Cash', 'Bank'],
                datasets: [{
                    data: [data.cash_balance, data.bank_balance],
                    backgroundColor: [expensesGradient, '#F59E0B'],
                    borderWidth: 0,
                    hoverOffset: 12
                }]
            },
            options: {
                responsive: true,
                cutout: '70%',
                plugins: { legend: { position: 'bottom', labels: { usePointStyle: true, padding: 20 } } }
            }
        });
    }
}

async function showDashboard() {
    showView('dashboardView');
    showSkeletonLoaders(true);
    
    try {
        const res = await fetchWithTimeout("http://127.0.0.1:8000/report/dashboard", {}, 5000);
        const data = await res.json();

        document.getElementById("dashSalesToday").innerText = formatMoney(data.sales_today);
        document.getElementById("dashSalesMonth").innerText = formatMoney(data.sales_month);
        document.getElementById("dashCash").innerText = formatMoney(data.cash_balance);
        document.getElementById("dashBank").innerText = formatMoney(data.bank_balance);
        document.getElementById("dashReceivables").innerText = formatMoney(data.receivables);
        const totalFunds = (Number(data.cash_balance || 0) + Number(data.bank_balance || 0));
        const cashRatio = totalFunds > 0 ? (Number(data.cash_balance || 0) / totalFunds) * 100 : 0;
        const totalFundsEl = document.getElementById("dashTotalFunds");
        const cashRatioEl = document.getElementById("dashCashRatio");
        if (totalFundsEl) totalFundsEl.innerText = formatMoney(totalFunds);
        if (cashRatioEl) cashRatioEl.innerText = `${cashRatio.toFixed(1)}%`;

        renderCharts(data);
        updateSystemStatus(true);
        showSkeletonLoaders(false);
    } catch (error) {
        showSkeletonLoaders(false);
        if (error.name !== 'AbortError') {
            showToast('Connection error: ' + error.message + '. Retrying...', 'error');
        } else {
            showToast('Dashboard request timed out. Retrying...', 'error');
        }
        updateSystemStatus(false);
        setTimeout(() => showDashboard(), 2000);
    }
}



function showSkeletonLoaders(show = true) {
    const loaders = document.querySelectorAll('.skeleton-loader');
    loaders.forEach(loader => {
        loader.style.display = show ? 'block' : 'none';
    });
}

function isViewActive(viewId) {
    const view = document.getElementById(viewId);
    return !!(view && view.classList.contains('active'));
}

async function updateDashboard() {
    try {
        const res = await fetchWithTimeout("http://127.0.0.1:8000/report/dashboard");
        const data = await res.json();

        const salesToday = document.getElementById("dashSalesToday");
        const salesMonth = document.getElementById("dashSalesMonth");
        const cash = document.getElementById("dashCash");
        const bank = document.getElementById("dashBank");
        const receivables = document.getElementById("dashReceivables");

        if (salesToday) salesToday.innerText = formatMoney(data.sales_today || 0);
        if (salesMonth) salesMonth.innerText = formatMoney(data.sales_month || 0);
        if (cash) cash.innerText = formatMoney(data.cash_balance || 0);
        if (bank) bank.innerText = formatMoney(data.bank_balance || 0);
        if (receivables) receivables.innerText = formatMoney(data.receivables || 0);
        const totalFunds = (Number(data.cash_balance || 0) + Number(data.bank_balance || 0));
        const cashRatio = totalFunds > 0 ? (Number(data.cash_balance || 0) / totalFunds) * 100 : 0;
        const totalFundsEl = document.getElementById("dashTotalFunds");
        const cashRatioEl = document.getElementById("dashCashRatio");
        if (totalFundsEl) totalFundsEl.innerText = formatMoney(totalFunds);
        if (cashRatioEl) cashRatioEl.innerText = `${cashRatio.toFixed(1)}%`;

        const salesCanvas = document.getElementById('salesChart');
        const expenseCanvas = document.getElementById('expenseChart');
        if (salesCanvas && expenseCanvas) {
            renderCharts(data);
        }
        updateSystemStatus(true);
    } catch (e) {
        updateSystemStatus(false);
    }
}

function updateSystemStatus(isOnline) {
    const backendEl = document.getElementById('statusBackend');
    const backupEl = document.getElementById('statusBackup');
    const uptimeEl = document.getElementById('statusUptime');

    localStorage.setItem('backendStatus', isOnline ? 'online' : 'offline');
    if (backendEl) {
        backendEl.textContent = isOnline ? 'Online' : 'Offline';
        backendEl.classList.toggle('status-online', isOnline);
        backendEl.classList.toggle('status-offline', !isOnline);
    }

    const lastBackup = localStorage.getItem('lastBackupAt');
    if (backupEl) {
        backupEl.textContent = lastBackup ? new Date(lastBackup).toLocaleString() : 'Never';
    }

    if (uptimeEl) {
        if (!isOnline) {
            uptimeEl.textContent = 'Stopped';
        } else {
            const seconds = Math.floor((Date.now() - appStartTime) / 1000);
            const mins = Math.floor(seconds / 60);
            const hrs = Math.floor(mins / 60);
            const uptimeText = hrs > 0 ? `${hrs}h ${mins % 60}m` : `${mins}m ${seconds % 60}s`;
            uptimeEl.textContent = uptimeText;
        }
    }
}

// Search Filter
function filterTable(tableId, query, isTbody = false) {
    const filter = query.toUpperCase();
    const table = document.getElementById(tableId);
    let tr = table.getElementsByTagName("tr");

    // If ID is tbody directly
    if (isTbody) {
        tr = document.getElementById(tableId).getElementsByTagName("tr");
    }

    for (let i = 0; i < tr.length; i++) {
        // Skip header if it's a full table
        if (!isTbody && tr[i].parentNode.nodeName === "THEAD") continue;
        if (tr[i].parentNode.nodeName === "TFOOT") continue;

        let visible = false;
        const tds = tr[i].getElementsByTagName("td");
        for (let j = 0; j < tds.length; j++) {
            if (tds[j]) {
                const txtValue = tds[j].textContent || tds[j].innerText;
                if (txtValue.toUpperCase().indexOf(filter) > -1) {
                    visible = true;
                    break;
                }
            }
        }
        tr[i].style.display = visible ? "" : "none";
    }
}

// Toast Notifications
const TOAST_DEDUPE_MS = 1200;
const MAX_TOASTS = 4;
let lastToastMeta = { key: "", ts: 0 };

function showToast(message, type = "info") {
    const container = document.getElementById("toast-container");
    if (!container) return;

    const safeMessage = String(message || "").trim();
    if (!safeMessage) return;

    const now = Date.now();
    const key = `${type}|${safeMessage}`;
    if (lastToastMeta.key === key && (now - lastToastMeta.ts) < TOAST_DEDUPE_MS) {
        return;
    }
    lastToastMeta = { key, ts: now };

    while (container.children.length >= MAX_TOASTS) {
        container.removeChild(container.firstElementChild);
    }

    const toast = document.createElement("div");
    toast.className = `toast ${type}`;
    toast.innerHTML = type === "success" ? `<ion-icon name="checkmark-circle"></ion-icon> ${safeMessage}` :
        type === "error" ? `<ion-icon name="alert-circle"></ion-icon> ${safeMessage}` : safeMessage;

    container.appendChild(toast);

    setTimeout(() => {
        toast.style.animation = "fadeOut 0.3s ease-out forwards";
        setTimeout(() => toast.remove(), 300);
    }, 3000);
}

function formatMoney(value) {
    const num = Number(value || 0);
    return num.toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 });
}

function formatDateShort(dateStr) {
    const d = new Date(dateStr);
    if (Number.isNaN(d.getTime())) return dateStr;
    const dd = String(d.getDate()).padStart(2, '0');
    const mm = String(d.getMonth() + 1).padStart(2, '0');
    const yyyy = String(d.getFullYear());
    return `${dd}-${mm}-${yyyy}`;
}

function formatDateInput(date) {
    return date.toISOString().slice(0, 10);
}

function setReportRange(prefix, days) {
    const toEl = document.getElementById(`${prefix}To`);
    const fromEl = document.getElementById(`${prefix}From`);
    if (!toEl || !fromEl) return;
    const end = new Date();
    const start = new Date();
    start.setDate(end.getDate() - (days - 1));
    toEl.value = formatDateInput(end);
    fromEl.value = formatDateInput(start);
    sessionStorage.setItem(`${prefix}ReportRange`, JSON.stringify({
        from: fromEl.value,
        to: toEl.value
    }));
}

function getReportRange(prefix) {
    const toEl = document.getElementById(`${prefix}To`);
    const fromEl = document.getElementById(`${prefix}From`);
    if (!toEl || !fromEl) return null;

    if (fromEl.value && toEl.value) {
        return { from: fromEl.value, to: toEl.value };
    }

    const stored = sessionStorage.getItem(`${prefix}ReportRange`);
    if (stored) {
        try {
            const parsed = JSON.parse(stored);
            if (parsed && parsed.from && parsed.to) {
                fromEl.value = parsed.from;
                toEl.value = parsed.to;
                return { from: parsed.from, to: parsed.to };
            }
        } catch (e) {
            // ignore parse errors
        }
    }

    setReportRange(prefix, 30);
    return { from: fromEl.value, to: toEl.value };
}

function buildReportQuery(prefix) {
    const range = getReportRange(prefix);
    if (!range) return "";
    const params = new URLSearchParams({
        start: range.from,
        end: range.to
    });
    return `?${params.toString()}`;
}

async function requestUpdateCheck() {
    const statusEl = document.getElementById('updateStatus');
    const lastCheckedEl = document.getElementById('updateLastChecked');
    const restartBtn = document.getElementById('updateRestartBtn');
    if (statusEl) statusEl.textContent = 'Checking for updates...';
    try {
        const res = await electronAPI.invoke('update:check');
        if (statusEl) {
            statusEl.textContent = res && res.status ? res.status : 'Update check completed.';
        }
        if (restartBtn) restartBtn.style.display = 'none';
    } catch (e) {
        if (statusEl) statusEl.textContent = 'Update check failed.';
    } finally {
        if (lastCheckedEl) lastCheckedEl.textContent = `Last checked: ${new Date().toLocaleString()}`;
    }
}

async function requestUpdateRestart() {
    const statusEl = document.getElementById('updateStatus');
    try {
        const res = await electronAPI.invoke('update:restart');
        if (statusEl) statusEl.textContent = res && res.status ? res.status : 'Restarting...';
    } catch (e) {
        if (statusEl) statusEl.textContent = 'Restart failed.';
    }
}

async function initUpdateBadge() {
    const badge = document.getElementById('updateVersionBadge');
    if (!badge) return;
    try {
        const version = await electronAPI.invoke('app:getVersion');
        badge.textContent = `Version: v${version}`;
    } catch (e) {
        badge.textContent = 'Version: --';
    }
}

electronAPI.onUpdateStatus((payload) => {
    const statusEl = document.getElementById('updateStatus');
    const restartBtn = document.getElementById('updateRestartBtn');
    if (statusEl && payload && payload.message) {
        statusEl.textContent = payload.message;
    }
    if (restartBtn) {
        restartBtn.style.display = payload && payload.type === 'downloaded' ? 'inline-flex' : 'none';
    }
});

async function requestUpdateCheck() {
    const statusEl = document.getElementById('updateStatus');
    const lastCheckedEl = document.getElementById('updateLastChecked');
    if (statusEl) statusEl.textContent = 'Checking for updates...';
    try {
        const res = await electronAPI.invoke('update:check');
        if (statusEl) {
            statusEl.textContent = res && res.status ? res.status : 'Update check completed.';
        }
    } catch (e) {
        if (statusEl) statusEl.textContent = 'Update check failed.';
    } finally {
        if (lastCheckedEl) lastCheckedEl.textContent = `Last checked: ${new Date().toLocaleString()}`;
    }
}

function applyTheme(theme) {
    const selected = (theme || 'white').toLowerCase();
    const effectiveTheme = (selected === 'splash' || selected === 'light') ? 'white' : selected;
    const root = document.documentElement;

    if (effectiveTheme === 'dark') {
        root.setAttribute('data-theme', 'dark');
        localStorage.setItem('theme', 'dark');
        electronAPI.invoke('window:setTheme', 'dark').catch(() => {});
    } else {
        root.setAttribute('data-theme', 'white');
        localStorage.setItem('theme', 'white');
        electronAPI.invoke('window:setTheme', 'white').catch(() => {});
    }

    const themeSelect = document.getElementById('themeSelect');
    if (themeSelect) {
        themeSelect.value = effectiveTheme === 'dark' ? 'dark' : 'white';
    }
}

function changeTheme() {
    const themeSelect = document.getElementById('themeSelect');
    applyTheme(themeSelect ? themeSelect.value : 'white');
}

// Initialize Theme
window.onload = function () {
    if (appInitDone) return;
    appInitDone = true;

    initApiBase().then(() => {
        startAutoBackup();
        checkAuth();
        // Load essential data after auth check
        if (sessionStorage.getItem('username')) {
            loadParties();
            loadTransactions();
        }
    });
    const status = localStorage.getItem('backendStatus');
    if (status === 'offline') {
        updateSystemStatus(false);
    }
    if (backendStatusIntervalId) clearInterval(backendStatusIntervalId);
    backendStatusIntervalId = setInterval(checkBackendStatus, 15000);
    const savedTheme = localStorage.getItem('theme') || 'white';
    applyTheme(savedTheme);
    applySidebarCollapsed(localStorage.getItem('sidebarCollapsed') === '1');

    initUpdateBadge();

    const dailyFrom = document.getElementById('dailyFrom');
    if (dailyFrom && !dailyFrom.value) {
        setReportRange('daily', 30);
    }
    const shortFrom = document.getElementById('shortFrom');
    if (shortFrom && !shortFrom.value) {
        setReportRange('short', 30);
    }
    const purchaseFrom = document.getElementById('purchaseFrom');
    if (purchaseFrom && !purchaseFrom.value) {
        setReportRange('purchase', 30);
    }

    initializeDatePickers();
    initSoftScroll();
    
    // Failsafe: periodically check if modals are blocking the app
    if (uiFailsafeIntervalId) clearInterval(uiFailsafeIntervalId);
    uiFailsafeIntervalId = setInterval(() => {
        if (sessionStorage.getItem('username')) {
            const editModal = document.getElementById('editTxnModal');
            const confirmModal = document.getElementById('confirmDeleteModal');
            const appContent = document.getElementById('appContent');
            
            // If modals are not shown but somehow blocking
            if (editModal) {
                const computedDisplay = window.getComputedStyle(editModal).display;
                if (computedDisplay !== 'flex' && computedDisplay !== 'block') {
                    editModal.style.pointerEvents = 'none';
                    editModal.style.visibility = 'hidden';
                }
            }
            
            if (confirmModal) {
                const computedDisplay = window.getComputedStyle(confirmModal).display;
                if (computedDisplay !== 'flex' && computedDisplay !== 'block') {
                    confirmModal.style.pointerEvents = 'none';
                    confirmModal.style.visibility = 'hidden';
                }
            }
            
            // Ensure app content is always interactive when logged in
            if (appContent && window.getComputedStyle(appContent).pointerEvents === 'none') {
                appContent.style.pointerEvents = 'auto';
                appContent.style.filter = 'none';
                console.warn('Failsafe: restored app interactivity');
            }
        }
    }, 2500);
}

window.addEventListener('beforeunload', () => {
    if (backendStatusIntervalId) {
        clearInterval(backendStatusIntervalId);
        backendStatusIntervalId = null;
    }
    if (uiFailsafeIntervalId) {
        clearInterval(uiFailsafeIntervalId);
        uiFailsafeIntervalId = null;
    }
    if (autoBackupTimer) {
        clearInterval(autoBackupTimer);
        autoBackupTimer = null;
    }
});

async function checkBackendStatus() {
    try {
        const controller = new AbortController();
        const timeout = setTimeout(() => controller.abort(), 2000);
        const res = await fetch("http://127.0.0.1:8000/report/dashboard", { signal: controller.signal });
        clearTimeout(timeout);
        if (res.ok) {
            updateSystemStatus(true);
        } else {
            updateSystemStatus(false);
        }
    } catch (e) {
        updateSystemStatus(false);
    }
}

// Other Reports (Keeping existing logic mostly same but adding Error Handling)
async function showDailySummary() {
    showView('dailySummaryView');
    const tbody = document.getElementById("dailySummaryBody");
    tbody.innerHTML = `<tr><td colspan="10" class="text-right">Loading...</td></tr>`;

    let data = [];
    try {
        const res = await fetchReport(`http://127.0.0.1:8000/report/daily-summary${buildReportQuery('daily')}`);
        data = await res.json();
    } catch (e) {
        showToast("Daily Summary failed: " + e.message, "error");
        tbody.innerHTML = `<tr><td colspan="10" class="text-right">No Data</td></tr>`;
        return;
    }

    tbody.innerHTML = "";

    if (!data || data.length === 0) {
        tbody.innerHTML = `<tr><td colspan="10" class="text-right">No Data</td></tr>`;
        return;
    }

    let rowsHtml = '';
    data.forEach(row => {
        let cashInHandCell = `<span>${formatMoney(row.closing_cash)}</span>`;
        if (currentRole === 'admin') {
            const inputVal = row.cash_in_hand === null || row.cash_in_hand === undefined ? '' : row.cash_in_hand;
            cashInHandCell = `
                <div style="display:flex; gap:6px; align-items:center; justify-content:flex-end;">
                    <input type="number" id="cashInHand-${row.date}" value="${inputVal}" style="width:120px; text-align:right;">
                    <button class="btn-sm" style="background:#4b5563; padding: 4px 8px; font-size: 12px; color:white; border:none; border-radius:4px; cursor:pointer;" onclick="saveCashInHand('${row.date}')">Save</button>
                    <button class="btn-sm" style="background:#9ca3af; padding: 4px 8px; font-size: 12px; color:white; border:none; border-radius:4px; cursor:pointer;" onclick="resetCashInHand('${row.date}')">Reset</button>
                </div>`;
        }

        const shortExcess = Number(row.cash_short_excess || 0);
        const shortExcessColor = shortExcess < 0 ? "var(--danger)" : shortExcess > 0 ? "var(--success)" : "inherit";

        rowsHtml += `
            <tr>
                <td>${formatDateShort(row.date)}</td>
                <td class="text-right">${formatMoney(row.opening_cash)}</td>
                <td class="text-right">${formatMoney(row.cash_in)}</td>
                <td class="text-right">${formatMoney(row.cash_expense)}</td>
                <td class="text-right">${formatMoney(row.cash_needed)}</td>
                <td class="text-right">${cashInHandCell}</td>
                <td class="text-right" style="color:${shortExcessColor};">${formatMoney(row.cash_short_excess)}</td>
                <td class="text-right">${formatMoney(row.bank)}</td>
                <td class="text-right">${formatMoney(row.credit_sale)}</td>
                <td class="text-right">${formatMoney(row.total_sales)}</td>
            </tr>`;
    });
    tbody.innerHTML = rowsHtml;
}

function showDayBook() {
    showView('dayBookView');
}

let dayBookRows = [];
const DAYBOOK_ROW_HEIGHT = 36;
const DAYBOOK_OVERSCAN = 8;
const DAYBOOK_STATIC_RENDER_THRESHOLD = 300;
let dayBookVirtualInitialized = false;

function initDayBookVirtualScroll() {
    if (dayBookVirtualInitialized) return;
    const container = document.getElementById('dayBookTableContainer');
    if (!container) return;
    dayBookVirtualInitialized = true;
    let scrollIdleTimer = null;
    let ticking = false;
    container.addEventListener('scroll', () => {
        container.classList.add('is-scrolling');
        if (scrollIdleTimer) clearTimeout(scrollIdleTimer);
        scrollIdleTimer = setTimeout(() => {
            container.classList.remove('is-scrolling');
        }, 90);
        if (ticking) return;
        ticking = true;
        requestAnimationFrame(() => {
            renderVirtualizedDayBook();
            ticking = false;
        });
    });
}

function renderVirtualizedDayBook(resetScroll = false) {
    const container = document.getElementById('dayBookTableContainer');
    const tbody = document.getElementById('dayBookBody');
    if (!container || !tbody) return;

    if (resetScroll) {
        container.scrollTop = 0;
    }

    const data = dayBookRows || [];
    const total = data.length;
    const containerHeight = container.clientHeight || 520;
    const scrollTop = container.scrollTop || 0;

    const measured = getMeasuredRowHeight(tbody, 'daybook-spacer', dayBookMeasuredRowHeight);
    dayBookMeasuredRowHeight = measured || dayBookMeasuredRowHeight;
    const rowHeight = dayBookMeasuredRowHeight;
    const useStaticRender = total <= DAYBOOK_STATIC_RENDER_THRESHOLD;
    const start = useStaticRender ? 0 : Math.max(0, Math.floor(scrollTop / rowHeight) - DAYBOOK_OVERSCAN);
    const visibleCount = useStaticRender ? total : Math.ceil(containerHeight / rowHeight) + DAYBOOK_OVERSCAN * 2;
    const end = Math.min(total, start + visibleCount);
    const renderKey = `${dayBookRenderVersion}:${currentRole || ''}:${start}:${end}:${total}`;
    if (!resetScroll && renderKey === dayBookLastRenderKey) {
        return;
    }
    dayBookLastRenderKey = renderKey;

    const topPad = useStaticRender ? 0 : start * rowHeight;
    const bottomPad = useStaticRender ? 0 : Math.max(0, (total - end) * rowHeight);
    const colspan = currentRole === 'admin' ? 7 : 6;

    const headerRow = document.querySelector('#dayBookTable thead tr');
    if (headerRow) {
        const existHeader = headerRow.querySelector('.action-header');
        if (existHeader) existHeader.remove();
        if (currentRole === 'admin') {
            const th = document.createElement('th');
            th.className = 'action-header';
            th.innerText = 'Action';
            headerRow.appendChild(th);
        }
    }

    let rowsHTML = '';
    if (topPad > 0) {
        rowsHTML += `<tr class="daybook-spacer"><td colspan="${colspan}" style="height:${topPad}px; border:none; padding:0;"></td></tr>`;
    }

    pruneTxnHighlights();
    for (let i = start; i < end; i += 1) {
        const row = data[i];
        const rowClass = getTxnHighlightClass(row.id);
        let actionCell = "";
        if (currentRole === 'admin') {
            actionCell = `<td class="action-cell">
                <button class="btn-action edit" onclick="openEditModal('${row.id}')" title="Edit">
                    <span class="material-symbols-rounded" aria-hidden="true">edit</span>
                    <span>Edit</span>
                </button>
                <button class="btn-action delete" onclick="if(!window.isDeleting) deleteTransaction('${row.id}')" title="Delete">
                    <span class="material-symbols-rounded" aria-hidden="true">delete</span>
                    <span>Delete</span>
                </button>
            </td>`;
        }

        rowsHTML += `
        <tr${rowClass ? ` class="${rowClass}"` : ''}>
            <td>${formatDateShort(row.date)}</td>
            <td>${row.bill_no || ''}</td>
            <td>${row.party}</td>
            <td>${row.type}</td>
            <td>${row.mode}</td>
            <td class="text-right">${formatMoney(row.amount)}</td>
            ${actionCell}
        </tr>`;
    }

    if (bottomPad > 0) {
        rowsHTML += `<tr class="daybook-spacer"><td colspan="${colspan}" style="height:${bottomPad}px; border:none; padding:0;"></td></tr>`;
    }

    tbody.innerHTML = rowsHTML;
}

async function loadDayBook() {
    const dateInput = document.getElementById('dayBookDate');
    const date = dateInput ? dateInput.value : '';
    if (!date) return showToast('Select a date', 'error');

    try {
        const res = await fetchWithTimeout(
            `http://127.0.0.1:8000/transactions/by-date?date=${encodeURIComponent(date)}`,
            {},
            5000,
            [404]
        );
        if (res.status === 404) {
            await loadDayBookFallback(date);
            return;
        }

        const data = await res.json();
        renderDayBookRows(data);
    } catch (e) {
        showToast('Error loading day book: ' + e, 'error');
    }
}

async function loadDayBookFallback(date) {
    const tbody = document.getElementById('dayBookBody');
    tbody.innerHTML = '';

    let page = 1;
    let totalPages = 1;
    const limit = 1000;
    const results = [];

    while (page <= totalPages) {
        const res = await fetchWithTimeout(`http://127.0.0.1:8000/transactions?page=${page}&limit=${limit}`);
        const response = await res.json();
        const rows = response.transactions || [];
        totalPages = response.total_pages || 1;

        rows.forEach(row => {
            if (row.date === date) {
                results.push(row);
            }
        });

        page += 1;
    }

    renderDayBookRows(results);
}

function renderDayBookRows(rows) {
    // Preserve backend ordering so Day Book matches transaction entry sequence.
    dayBookRows = (rows || []).slice();
    dayBookRenderVersion += 1;
    dayBookLastRenderKey = '';
    initDayBookVirtualScroll();
    renderVirtualizedDayBook(true);
}

function refreshDayBookIfNeeded() {
    const dateInput = document.getElementById('dayBookDate');
    const date = dateInput ? dateInput.value : '';
    if (!date) return;
    loadDayBook();
}

function exportDayBook() {
    const date = document.getElementById('dayBookDate').value;
    if (!date) return showToast('Select a date to export', 'error');
    exportTableToExcel('dayBookTable', `DayBook_${date}`, `DayBook_${date}`);
}

async function showShortReport() {
    showView('shortReportView');
    const tbody = document.getElementById("shortReportBody");
    const summaryDiv = document.getElementById("shortReportSummary");
    tbody.innerHTML = `<tr><td colspan="7" class="text-right">Loading...</td></tr>`;
    if (summaryDiv) summaryDiv.innerHTML = "";

    let data = [];
    try {
        const res = await fetchReport(`http://127.0.0.1:8000/report/short-excess${buildReportQuery('short')}`);
        data = await res.json();
    } catch (e) {
        showToast("Short Report failed: " + e.message, "error");
        tbody.innerHTML = `<tr><td colspan="7" class="text-right">No Data</td></tr>`;
        return;
    }

    tbody.innerHTML = "";

    if (!data || data.length === 0) {
        tbody.innerHTML = `<tr><td colspan="7" class="text-right">No Data</td></tr>`;
        return;
    }

    let totalShort = 0;
    const monthAgg = {};
    let bodyHtml = '';
    data.forEach(row => {
        const shortExcess = Number(row.cash_short_excess || 0);
        const shortExcessColor = shortExcess < 0 ? "var(--danger)" : shortExcess > 0 ? "var(--success)" : "inherit";
        const cashInHand = row.cash_in_hand === null || row.cash_in_hand === undefined ? 0 : row.cash_in_hand;

        if (shortExcess < 0) totalShort += shortExcess;

        const monthKey = row.date ? row.date.slice(0, 7) : "";
        if (monthKey) {
            if (!monthAgg[monthKey]) monthAgg[monthKey] = { short: 0, excess: 0 };
            if (shortExcess < 0) monthAgg[monthKey].short += shortExcess;
            if (shortExcess > 0) monthAgg[monthKey].excess += shortExcess;
        }

        bodyHtml += `
            <tr>
                <td>${formatDateShort(row.date)}</td>
                <td class="text-right">${formatMoney(row.opening_cash)}</td>
                <td class="text-right">${formatMoney(row.cash_in)}</td>
                <td class="text-right">${formatMoney(row.cash_expense)}</td>
                <td class="text-right">${formatMoney(row.cash_needed)}</td>
                <td class="text-right">${formatMoney(cashInHand)}</td>
                <td class="text-right" style="color:${shortExcessColor};">${formatMoney(row.cash_short_excess)}</td>
            </tr>`;
    });

    if (totalShort !== 0) {
        bodyHtml += `
            <tr style="font-weight:bold;">
                <td>Total Short</td>
                <td class="text-right"></td>
                <td class="text-right"></td>
                <td class="text-right"></td>
                <td class="text-right"></td>
                <td class="text-right"></td>
                <td class="text-right" style="color: var(--danger);">${formatMoney(totalShort)}</td>
            </tr>`;
    }
    tbody.innerHTML = bodyHtml;

    if (summaryDiv) {
        const months = Object.keys(monthAgg).sort().reverse();
        if (months.length) {
            let html = `
                <h3 style="margin: 16px 0 8px;">Monthly Short / Excess</h3>
                <table>
                    <thead>
                        <tr>
                            <th>Month</th>
                            <th class="text-right">Total Short</th>
                            <th class="text-right">Total Excess</th>
                            <th class="text-right">Net</th>
                        </tr>
                    </thead>
                    <tbody>`;
            months.forEach(m => {
                const shortVal = monthAgg[m].short;
                const excessVal = monthAgg[m].excess;
                const netVal = shortVal + excessVal;
                const netColor = netVal < 0 ? "var(--danger)" : netVal > 0 ? "var(--success)" : "inherit";
                html += `
                    <tr>
                        <td>${m}</td>
                        <td class="text-right" style="color: var(--danger);">${formatMoney(shortVal)}</td>
                        <td class="text-right" style="color: var(--success);">${formatMoney(excessVal)}</td>
                        <td class="text-right" style="color:${netColor};">${formatMoney(netVal)}</td>
                    </tr>`;
            });
            html += `</tbody></table>`;
            summaryDiv.innerHTML = html;
        }
    }
}

async function showPurchaseReport() {
    showView('purchaseReportView');
    const monthlyBody = document.getElementById('purchaseMonthlyBody');
    const supplierBody = document.getElementById('purchaseSupplierBody');
    const summaryEl = document.getElementById('purchaseSummary');

    if (monthlyBody) monthlyBody.innerHTML = `<tr><td colspan="2" class="text-right">Loading...</td></tr>`;
    if (supplierBody) supplierBody.innerHTML = `<tr><td colspan="2" class="text-right">Loading...</td></tr>`;
    if (summaryEl) summaryEl.innerHTML = '';

    let monthlyData = [];
    let supplierData = [];
    try {
        const partyInput = document.getElementById('purchaseParty');
        const partyName = partyInput ? partyInput.value.trim() : '';
        const baseQuery = buildReportQuery('purchase');
        const params = new URLSearchParams(baseQuery.replace(/^\?/, ''));
        if (partyName) {
            params.set('party', partyName);
        }
        const query = params.toString() ? `?${params.toString()}` : '';
        const [monthlyRes, supplierRes] = await Promise.all([
            fetchReport(`http://127.0.0.1:8000/report/purchase/monthly${query}`),
            fetchReport(`http://127.0.0.1:8000/report/purchase/supplier${query}`)
        ]);
        monthlyData = await monthlyRes.json();
        supplierData = await supplierRes.json();
    } catch (e) {
        showToast('Purchase report failed: ' + e.message, 'error');
        if (monthlyBody) monthlyBody.innerHTML = `<tr><td colspan="2" class="text-right">No Data</td></tr>`;
        if (supplierBody) supplierBody.innerHTML = `<tr><td colspan="2" class="text-right">No Data</td></tr>`;
        return;
    }

    if (monthlyBody) {
        monthlyBody.innerHTML = '';
        if (!monthlyData || monthlyData.length === 0) {
            monthlyBody.innerHTML = `<tr><td colspan="2" class="text-right">No Data</td></tr>`;
        } else {
            let monthlyTotal = 0;
            let monthlyHtml = '';
            monthlyData.forEach(row => {
                const amt = Number(row.total_amount || 0);
                monthlyTotal += amt;
                monthlyHtml += `
                    <tr>
                        <td>${row.month || ''}</td>
                        <td class="text-right">${formatMoney(amt)}</td>
                    </tr>`;
            });
            monthlyHtml += `
                <tr style="font-weight:bold;">
                    <td>Total</td>
                    <td class="text-right">${formatMoney(monthlyTotal)}</td>
                </tr>`;
            monthlyBody.innerHTML = monthlyHtml;
        }
    }

    purchaseSupplierData = supplierData || [];
    renderPurchaseSupplier();
}

let purchaseSupplierData = [];
const INVENTORY_STORAGE_PREFIX = 'inventoryStock';
const INVENTORY_PRODUCT_MASTER_PREFIX = 'inventoryProductMaster';
const INVENTORY_MAIL_PROFILE_KEY = 'inventoryMailProfile';
const INVENTORY_MAIL_SUBJECT_AUTO_PREFIX = 'Inventory Report - ';
let inventoryModel = { month: null, days: 31, rows: [] };
let inventorySaveTimer = null;
let inventoryMailSubjectHooked = false;

function renderPurchaseSupplier() {
    const supplierBody = document.getElementById('purchaseSupplierBody');
    const summaryEl = document.getElementById('purchaseSummary');
    if (!supplierBody) return;

    const sortEl = document.getElementById('purchaseSupplierSort');
    const sortValue = sortEl ? sortEl.value : 'name-asc';

    const sorted = (purchaseSupplierData || []).slice().sort((a, b) => {
        if (sortValue === 'name-desc') {
            return String(b.party || '').localeCompare(String(a.party || ''), 'en', { sensitivity: 'base' });
        }
        if (sortValue === 'amount-desc') {
            return Number(b.total_amount || 0) - Number(a.total_amount || 0);
        }
        if (sortValue === 'amount-asc') {
            return Number(a.total_amount || 0) - Number(b.total_amount || 0);
        }
        return String(a.party || '').localeCompare(String(b.party || ''), 'en', { sensitivity: 'base' });
    });

    supplierBody.innerHTML = '';
    if (!sorted.length) {
        supplierBody.innerHTML = `<tr><td colspan="2" class="text-right">No Data</td></tr>`;
        if (summaryEl) summaryEl.innerHTML = '';
        return;
    }

    let supplierTotal = 0;
    sorted.forEach(row => {
        const amt = Number(row.total_amount || 0);
        supplierTotal += amt;
        supplierBody.innerHTML += `
            <tr>
                <td>${row.party || ''}</td>
                <td class="text-right">${formatMoney(amt)}</td>
            </tr>`;
    });

    supplierBody.innerHTML += `
        <tr style="font-weight:bold;">
            <td>Total</td>
            <td class="text-right">${formatMoney(supplierTotal)}</td>
        </tr>`;

    if (summaryEl) {
        summaryEl.innerHTML = `
            <div style="padding: 12px 14px; background: var(--card-bg); border-radius: 8px; text-align:center; font-weight: 600;">
                Total Purchase: ${formatMoney(supplierTotal)} | Parties: ${sorted.length}
            </div>`;
    }
}

function showInventoryManagement() {
    showView('inventoryView');
}

function showInventoryValueReport() {
    showView('inventoryValueView');
}

function ensureInventoryValueMonthOptions() {
    ensureInventoryMonthOptions();
    const src = document.getElementById('inventoryMonthSelect');
    const dst = document.getElementById('inventoryValueMonthSelect');
    if (!src || !dst) return;
    if (!dst.options.length) {
        dst.innerHTML = src.innerHTML;
    }
    if (!dst.value) {
        dst.value = src.value || String(new Date().getMonth() + 1);
    }
}

function loadInventoryValueMonth() {
    ensureInventoryValueMonthOptions();
    const src = document.getElementById('inventoryMonthSelect');
    const dst = document.getElementById('inventoryValueMonthSelect');
    if (!src || !dst) return;

    if (src.value !== dst.value) {
        src.value = dst.value;
    }

    loadInventoryMonth();
    updateGlobalReportContext('inventoryValueView');
}

function getInventoryFinancialYear() {
    const fy = (sessionStorage.getItem('financialYear') || '').trim();
    if (/^\d{4}-\d{4}$/.test(fy)) return fy;
    const now = new Date();
    const y = now.getFullYear();
    const start = (now.getMonth() + 1) >= 4 ? y : y - 1;
    return `${start}-${start + 1}`;
}

function getInventoryYearForMonth(fy, monthNum) {
    const parts = String(fy).split('-').map(Number);
    const startYear = Number(parts[0]);
    const endYear = Number(parts[1]);
    return monthNum >= 4 ? startYear : endYear;
}

function getInventoryStorageKey(fy, monthNum) {
    return `${INVENTORY_STORAGE_PREFIX}:${fy}:${String(monthNum).padStart(2, '0')}`;
}

function getInventoryMasterKey(fy) {
    return `${INVENTORY_PRODUCT_MASTER_PREFIX}:${fy}`;
}

function loadInventoryProductMaster(fy) {
    let rows = [];
    try {
        const raw = JSON.parse(localStorage.getItem(getInventoryMasterKey(fy)) || '[]');
        rows = Array.isArray(raw) ? raw : [];
    } catch (_) {
        rows = [];
    }

    const seen = new Set();
    const cleaned = [];
    rows.forEach((row) => {
        const name = String((row && row.name) || '').trim();
        if (!name) return;
        const key = name.toLowerCase();
        if (seen.has(key)) return;
        seen.add(key);
        const minStock = Number(row && row.min_stock);
        const cost = Number(row && row.cost);
        cleaned.push({
            name,
            min_stock: Number.isFinite(minStock) && minStock > 0 ? minStock : 0,
            cost: Number.isFinite(cost) && cost > 0 ? cost : 0
        });
    });

    return cleaned;
}

function saveInventoryProductMaster(fy, rows) {
    const source = Array.isArray(rows) ? rows : [];
    const seen = new Set();
    const master = [];

    source.forEach((row) => {
        const name = String((row && row.name) || '').trim();
        if (!name) return;
        const key = name.toLowerCase();
        if (seen.has(key)) return;
        seen.add(key);
        const minStock = Number(row && row.min_stock);
        const cost = Number(row && row.cost);
        master.push({
            name,
            min_stock: Number.isFinite(minStock) && minStock > 0 ? minStock : 0,
            cost: Number.isFinite(cost) && cost > 0 ? cost : 0
        });
    });

    localStorage.setItem(getInventoryMasterKey(fy), JSON.stringify(master));
}

function mergeInventoryRowsWithMaster(monthRows, masterRows, dayCount) {
    const merged = sanitizeInventoryRows(monthRows, dayCount);
    const existing = new Map();
    merged.forEach((row, idx) => {
        const key = String(row.name || '').trim().toLowerCase();
        if (key) existing.set(key, idx);
    });

    (Array.isArray(masterRows) ? masterRows : []).forEach((m) => {
        const name = String((m && m.name) || '').trim();
        if (!name) return;
        const key = name.toLowerCase();
        if (existing.has(key)) {
            const idx = existing.get(key);
            if (idx >= 0 && merged[idx]) {
                const curMin = Number(merged[idx].min_stock || 0);
                const masterMin = Number(m.min_stock || 0);
                const masterCost = Number(m.cost || 0);
                if (curMin <= 0 && masterMin > 0) {
                    merged[idx].min_stock = masterMin;
                }
                if (Number(merged[idx].cost || 0) <= 0 && masterCost > 0) {
                    merged[idx].cost = masterCost;
                }
            }
            return;
        }

        merged.push({
            name,
            qty: Array.from({ length: dayCount }, () => 0),
            min_stock: Number(m.min_stock || 0) > 0 ? Number(m.min_stock || 0) : 0,
            cost: Number(m.cost || 0) > 0 ? Number(m.cost || 0) : 0
        });
        existing.set(key, merged.length - 1);
    });

    return merged;
}

function ensureInventoryMonthOptions() {
    const monthSelect = document.getElementById('inventoryMonthSelect');
    if (!monthSelect || monthSelect.options.length) return;

    const monthNames = [
        'January', 'February', 'March', 'April', 'May', 'June',
        'July', 'August', 'September', 'October', 'November', 'December'
    ];
    monthSelect.innerHTML = monthNames
        .map((name, idx) => `<option value="${idx + 1}">${name}</option>`)
        .join('');

    monthSelect.value = String(new Date().getMonth() + 1);
}

function sanitizeInventoryRows(rows, dayCount) {
    if (!Array.isArray(rows)) return [];
    return rows.map((row) => {
        const qty = Array.from({ length: dayCount }, (_, i) => {
            const val = Number((row && Array.isArray(row.qty)) ? row.qty[i] : 0);
            return Number.isFinite(val) ? val : 0;
        });
        const minStock = Number(row && row.min_stock);
        const cost = Number(row && row.cost);
        return {
            name: String((row && row.name) || '').trim(),
            qty,
            min_stock: Number.isFinite(minStock) && minStock > 0 ? minStock : 0,
            cost: Number.isFinite(cost) && cost > 0 ? cost : 0
        };
    });
}

function loadInventoryMonth() {
    if (inventorySaveTimer) {
        clearTimeout(inventorySaveTimer);
        inventorySaveTimer = null;
    }
    ensureInventoryMonthOptions();
    const monthSelect = document.getElementById('inventoryMonthSelect');
    if (!monthSelect) return;

    const monthNum = Number(monthSelect.value || (new Date().getMonth() + 1));
    const fy = getInventoryFinancialYear();
    const yearForMonth = getInventoryYearForMonth(fy, monthNum);
    const dayCount = new Date(yearForMonth, monthNum, 0).getDate();
    const storageKey = getInventoryStorageKey(fy, monthNum);

    let saved = null;
    try {
        saved = JSON.parse(localStorage.getItem(storageKey) || 'null');
    } catch (_) {
        saved = null;
    }

    inventoryModel = {
        month: monthNum,
        days: dayCount,
        rows: mergeInventoryRowsWithMaster(
            saved && saved.rows,
            loadInventoryProductMaster(fy),
            dayCount
        )
    };

    initInventoryMailSubjectHook();
    syncInventoryMailSubjectWithMonth();
    renderInventoryTable();
    updateGlobalReportContext('inventoryView');
}

function getInventoryAutoSubjectText() {
    const monthSelect = document.getElementById('inventoryMonthSelect');
    const monthText = monthSelect && monthSelect.options.length
        ? monthSelect.options[monthSelect.selectedIndex].text
        : 'Month';
    return `${INVENTORY_MAIL_SUBJECT_AUTO_PREFIX}${monthText} (${getInventoryFinancialYear()})`;
}

function initInventoryMailSubjectHook() {
    if (inventoryMailSubjectHooked) return;
    const subjectEl = document.getElementById('inventoryMailSubject');
    if (!subjectEl) return;

    subjectEl.addEventListener('input', () => {
        const autoText = getInventoryAutoSubjectText();
        const val = String(subjectEl.value || '').trim();
        subjectEl.dataset.autoSubject = val && val === autoText ? '1' : '0';
    });

    inventoryMailSubjectHooked = true;
}

function syncInventoryMailSubjectWithMonth(force = false) {
    const subjectEl = document.getElementById('inventoryMailSubject');
    if (!subjectEl) return;
    const autoText = getInventoryAutoSubjectText();
    const current = String(subjectEl.value || '').trim();
    const isAuto = subjectEl.dataset.autoSubject === '1';

    if (force || !current || isAuto) {
        subjectEl.value = autoText;
        subjectEl.dataset.autoSubject = '1';
    }
}

function renderInventoryTable() {
    const thead = document.getElementById('inventoryHead');
    const tbody = document.getElementById('inventoryBody');
    const summary = document.getElementById('inventorySummary');
    if (!thead || !tbody || !summary) return;

    const mm = String(inventoryModel.month || 1).padStart(2, '0');
    let headerHtml = '<tr><th class="inventory-sticky-col">PRODUCT NAME</th><th class="text-right">COST</th><th class="text-right">MIN STOCK</th>';
    for (let d = 1; d <= inventoryModel.days; d += 1) {
        headerHtml += `<th class="text-right">${d}.${mm}</th>`;
    }
    headerHtml += '</tr>';
    thead.innerHTML = headerHtml;

    if (!inventoryModel.rows.length) {
        tbody.innerHTML = `<tr><td colspan="${inventoryModel.days + 3}" class="text-right">No products yet. Add product or import sheet.</td></tr>`;
        summary.textContent = '0 products | Total qty: 0.00 | Current stock value: 0.00';
        renderInventoryValueReport();
        return;
    }

    let bodyHtml = '';
    inventoryModel.rows.forEach((row, rowIndex) => {
        bodyHtml += `<tr><td class="inventory-sticky-col"><input class="inventory-product-input" data-row="${rowIndex}" data-col="0" data-kind="name" value="${escapeHtml(row.name)}" placeholder="Product name"></td>`;
        bodyHtml += `<td><input type="number" class="inventory-cell-input" data-row="${rowIndex}" data-col="1" data-kind="cost" value="${Number(row.cost || 0) === 0 ? '' : Number(row.cost || 0)}" min="0" step="0.01" placeholder="0"></td>`;
        bodyHtml += `<td><input type="number" class="inventory-cell-input" data-row="${rowIndex}" data-col="2" data-kind="min_stock" value="${Number(row.min_stock || 0) === 0 ? '' : Number(row.min_stock || 0)}" min="0" step="1" placeholder="0"></td>`;
        for (let dayIndex = 0; dayIndex < inventoryModel.days; dayIndex += 1) {
            const value = Number(row.qty[dayIndex] || 0);
            bodyHtml += `<td><input type="number" class="inventory-cell-input" data-row="${rowIndex}" data-col="${3 + dayIndex}" data-day="${dayIndex}" data-kind="qty" value="${value === 0 ? '' : value}" min="0" step="1"></td>`;
        }
        bodyHtml += '</tr>';
    });
    tbody.innerHTML = bodyHtml;
    refreshInventorySummary();

    tbody.querySelectorAll('input[data-kind="name"]').forEach((input) => {
        input.addEventListener('input', (e) => {
            const idx = Number(e.target.getAttribute('data-row'));
            if (!Number.isInteger(idx) || !inventoryModel.rows[idx]) return;
            inventoryModel.rows[idx].name = String(e.target.value || '').trimStart();
            queueInventoryAutoSave();
        });
    });

    tbody.querySelectorAll('input[data-kind="qty"]').forEach((input) => {
        input.addEventListener('focus', (e) => {
            e.target.select();
        });
        input.addEventListener('input', (e) => {
            const rowIdx = Number(e.target.getAttribute('data-row'));
            const dayIdx = Number(e.target.getAttribute('data-day'));
            if (!Number.isInteger(rowIdx) || !Number.isInteger(dayIdx) || !inventoryModel.rows[rowIdx]) return;
            const val = Number(e.target.value);
            inventoryModel.rows[rowIdx].qty[dayIdx] = Number.isFinite(val) ? val : 0;
            refreshInventorySummary();
            renderInventoryValueReport();
            queueInventoryAutoSave();
        });
    });

    tbody.querySelectorAll('input[data-kind="min_stock"]').forEach((input) => {
        input.addEventListener('focus', (e) => {
            e.target.select();
        });
        input.addEventListener('input', (e) => {
            const rowIdx = Number(e.target.getAttribute('data-row'));
            if (!Number.isInteger(rowIdx) || !inventoryModel.rows[rowIdx]) return;
            const val = Number(e.target.value);
            inventoryModel.rows[rowIdx].min_stock = Number.isFinite(val) && val > 0 ? val : 0;
            queueInventoryAutoSave();
        });
    });

    tbody.querySelectorAll('input[data-kind="cost"]').forEach((input) => {
        input.addEventListener('focus', (e) => {
            e.target.select();
        });
        input.addEventListener('input', (e) => {
            const rowIdx = Number(e.target.getAttribute('data-row'));
            if (!Number.isInteger(rowIdx) || !inventoryModel.rows[rowIdx]) return;
            const val = Number(e.target.value);
            inventoryModel.rows[rowIdx].cost = Number.isFinite(val) && val > 0 ? val : 0;
            refreshInventorySummary();
            renderInventoryValueReport();
            queueInventoryAutoSave();
        });
    });

    tbody.querySelectorAll('input[data-row][data-col]').forEach((input) => {
        input.addEventListener('keydown', handleInventoryCellNavigation);
    });

    renderInventoryValueReport();
}

function computeInventoryTotals() {
    let totalQty = 0;
    let currentStockValue = 0;
    const lastDayIndex = Math.max((inventoryModel.days || 1) - 1, 0);
    (inventoryModel.rows || []).forEach((row) => {
        const qty = Array.isArray(row.qty) ? row.qty : [];
        qty.forEach((v) => {
            totalQty += Number(v || 0);
        });
        const currentQty = Number(qty[lastDayIndex] || 0);
        currentStockValue += currentQty * Number(row.cost || 0);
    });
    return { totalQty, currentStockValue };
}

function refreshInventorySummary() {
    const summary = document.getElementById('inventorySummary');
    if (!summary) return;
    const { totalQty, currentStockValue } = computeInventoryTotals();
    summary.textContent = `${inventoryModel.rows.length} products | Total qty: ${formatMoney(totalQty)} | Current stock value: ${formatMoney(currentStockValue)}`;
}

function focusInventoryCell(rowIdx, colIdx) {
    const cell = document.querySelector(`#inventoryBody input[data-row="${rowIdx}"][data-col="${colIdx}"]`);
    if (!cell) return false;
    cell.focus();
    if (typeof cell.select === 'function') cell.select();
    return true;
}

function appendInventoryRow(name = '') {
    inventoryModel.rows.push({
        name: String(name || '').trim(),
        qty: Array.from({ length: inventoryModel.days }, () => 0),
        min_stock: 0,
        cost: 0
    });
}

function handleInventoryCellNavigation(event) {
    const key = event.key;
    const rowIdx = Number(event.target.getAttribute('data-row'));
    const colIdx = Number(event.target.getAttribute('data-col'));
    if (!Number.isInteger(rowIdx) || !Number.isInteger(colIdx)) return;

    let targetRow = rowIdx;
    let targetCol = colIdx;
    let shouldNavigate = false;

    if (key === 'Enter' && !event.altKey && !event.ctrlKey && !event.metaKey) {
        shouldNavigate = true;
        targetRow += event.shiftKey ? -1 : 1;
    } else if (event.altKey && !event.ctrlKey && !event.metaKey) {
        if (key === 'ArrowDown') {
            shouldNavigate = true;
            targetRow += 1;
        } else if (key === 'ArrowUp') {
            shouldNavigate = true;
            targetRow -= 1;
        } else if (key === 'ArrowRight') {
            shouldNavigate = true;
            targetCol += 1;
        } else if (key === 'ArrowLeft') {
            shouldNavigate = true;
            targetCol -= 1;
        }
    }

    if (!shouldNavigate) return;
    event.preventDefault();

    const maxCol = inventoryModel.days + 2;
    targetCol = Math.max(0, Math.min(maxCol, targetCol));

    if (targetRow < 0) {
        targetRow = 0;
    } else if (targetRow >= inventoryModel.rows.length) {
        appendInventoryRow('');
        renderInventoryTable();
        queueInventoryAutoSave();
        targetRow = inventoryModel.rows.length - 1;
    }

    focusInventoryCell(targetRow, targetCol);
}

function renderInventoryValueReport() {
    const body = document.getElementById('inventoryValueBody');
    const summary = document.getElementById('inventoryValueSummary');
    const totalEl = document.getElementById('inventoryValueTotal');
    if (!body || !summary) return;

    if (!inventoryModel.rows.length || !inventoryModel.days) {
        body.innerHTML = '<tr><td colspan="3" class="text-right">No value report available</td></tr>';
        summary.textContent = 'No value report available';
        if (totalEl) totalEl.textContent = 'Total Stock Value: 0.00';
        return;
    }

    let html = '';
    let monthValueTotal = 0;
    let latestDayValue = 0;
    for (let dayIndex = 0; dayIndex < inventoryModel.days; dayIndex += 1) {
        let dayQtyTotal = 0;
        let dayValueTotal = 0;
        inventoryModel.rows.forEach((row) => {
            const qty = Number((row.qty || [])[dayIndex] || 0);
            const cost = Number(row.cost || 0);
            dayQtyTotal += qty;
            dayValueTotal += qty * cost;
        });

        monthValueTotal += dayValueTotal;
        if (dayIndex === inventoryModel.days - 1) {
            latestDayValue = dayValueTotal;
        }
        html += `<tr>
            <td>${dayIndex + 1}.${String(inventoryModel.month || 1).padStart(2, '0')}</td>
            <td class="text-right">${formatMoney(dayQtyTotal)}</td>
            <td class="text-right">${formatMoney(dayValueTotal)}</td>
        </tr>`;
    }

    body.innerHTML = html;
    const avgDayValue = monthValueTotal / Math.max(inventoryModel.days, 1);
    summary.textContent = `Days: ${inventoryModel.days} | Avg daily stock value: ${formatMoney(avgDayValue)}`;
    if (totalEl) {
        totalEl.textContent = `Total Stock Value (Latest Day): ${formatMoney(latestDayValue)}`;
    }
}

function escapeHtml(text) {
    return String(text || '')
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

function addInventoryRow() {
    const input = document.getElementById('inventoryProductInput');
    const name = (input ? input.value : '').trim();
    appendInventoryRow(name);
    if (input) input.value = '';
    renderInventoryTable();
    saveInventorySnapshot(true);
}

function removeEmptyInventoryRows() {
    const before = inventoryModel.rows.length;
    inventoryModel.rows = inventoryModel.rows.filter((row) => {
        const hasName = String(row.name || '').trim().length > 0;
        const hasQty = Array.isArray(row.qty) && row.qty.some((v) => Number(v || 0) !== 0);
        const hasMinStock = Number(row.min_stock || 0) > 0;
        const hasCost = Number(row.cost || 0) > 0;
        return hasName || hasQty || hasMinStock || hasCost;
    });
    renderInventoryTable();
    if (before !== inventoryModel.rows.length) {
        saveInventorySnapshot(true);
        showToast('Empty rows removed', 'success');
    }
}

function queueInventoryAutoSave() {
    if (inventorySaveTimer) clearTimeout(inventorySaveTimer);
    const monthSelect = document.getElementById('inventoryMonthSelect');
    const monthNum = Number(monthSelect ? monthSelect.value : (inventoryModel.month || 1));
    const fy = getInventoryFinancialYear();
    const snapshotPayload = {
        fy,
        month: Number.isFinite(monthNum) && monthNum >= 1 && monthNum <= 12 ? monthNum : Number(inventoryModel.month || 1),
        rows: sanitizeInventoryRows(inventoryModel.rows, inventoryModel.days),
        updated_at: new Date().toISOString()
    };
    inventorySaveTimer = setTimeout(() => {
        inventorySaveTimer = null;
        persistInventoryPayload(snapshotPayload, true);
    }, 400);
}

function persistInventoryPayload(payload, silent = false) {
    const fy = String(payload && payload.fy ? payload.fy : getInventoryFinancialYear());
    const monthNum = Number(payload && payload.month ? payload.month : (inventoryModel.month || 1));
    if (!Number.isFinite(monthNum) || monthNum < 1 || monthNum > 12) return;
    const key = getInventoryStorageKey(fy, monthNum);
    const safeRows = sanitizeInventoryRows(payload && payload.rows ? payload.rows : inventoryModel.rows, inventoryModel.days);
    const finalPayload = {
        fy,
        month: monthNum,
        rows: safeRows,
        updated_at: payload && payload.updated_at ? payload.updated_at : new Date().toISOString()
    };
    localStorage.setItem(key, JSON.stringify(finalPayload));
    saveInventoryProductMaster(fy, safeRows);
    if (!silent) {
        showToast('Inventory saved', 'success');
    }
}

function saveInventorySnapshot(silent = false) {
    ensureInventoryMonthOptions();
    const monthSelect = document.getElementById('inventoryMonthSelect');
    const monthNum = Number(monthSelect ? monthSelect.value : (inventoryModel.month || 1));
    const fy = getInventoryFinancialYear();
    const payload = {
        fy,
        month: monthNum,
        rows: sanitizeInventoryRows(inventoryModel.rows, inventoryModel.days),
        updated_at: new Date().toISOString()
    };
    persistInventoryPayload(payload, silent);
}

async function importInventorySheet(event) {
    const file = event && event.target && event.target.files ? event.target.files[0] : null;
    if (!file) return;
    if (!XLSX) {
        showToast('XLSX parser is not available in this build', 'error');
        return;
    }

    try {
        const buffer = await file.arrayBuffer();
        const wb = XLSX.read(buffer, { type: 'array' });
        const firstSheetName = wb.SheetNames && wb.SheetNames[0];
        if (!firstSheetName) {
            showToast('No sheet found in file', 'error');
            return;
        }

        const sheet = wb.Sheets[firstSheetName];
        const rows = XLSX.utils.sheet_to_json(sheet, { header: 1, defval: '' });
        if (!Array.isArray(rows) || rows.length < 2) {
            showToast('Sheet is empty or invalid', 'error');
            return;
        }

        const header = rows[0].map((h) => String(h || '').trim());
        let productCol = header.findIndex((h) => h.toLowerCase() === 'product name');
        if (productCol < 0) {
            productCol = 0;
        }
        const costCol = header.findIndex((h) => {
            const k = String(h || '').trim().toLowerCase();
            return k === 'cost' || k === 'unit cost' || k === 'price';
        });

        const monthSelect = document.getElementById('inventoryMonthSelect');
        const headerMonth = header
            .map((h) => /^\d{1,2}[.\/-](\d{1,2})$/.exec(h))
            .find(Boolean);
        if (headerMonth && monthSelect) {
            const m = Number(headerMonth[1]);
            if (m >= 1 && m <= 12) {
                monthSelect.value = String(m);
            }
        }

        loadInventoryMonth();

        const importedRows = [];
        for (let i = 1; i < rows.length; i += 1) {
            const row = rows[i] || [];
            const name = String(row[productCol] || '').trim();
            if (!name) continue;

            const qty = Array.from({ length: inventoryModel.days }, () => 0);
            header.forEach((colName, colIdx) => {
                if (colIdx === productCol) return;
                const match = /^(\d{1,2})(?:[.\/-](\d{1,2}))?$/.exec(colName);
                if (!match) return;
                const day = Number(match[1]);
                if (!day || day < 1 || day > inventoryModel.days) return;
                const raw = String(row[colIdx] ?? '').replace(/,/g, '').trim();
                const val = Number(raw);
                if (Number.isFinite(val)) {
                    qty[day - 1] = val;
                }
            });

            let cost = 0;
            if (costCol >= 0) {
                const rawCost = String(row[costCol] ?? '').replace(/,/g, '').trim();
                const parsedCost = Number(rawCost);
                if (Number.isFinite(parsedCost) && parsedCost > 0) {
                    cost = parsedCost;
                }
            }

            importedRows.push({ name, qty, min_stock: 0, cost });
        }

        if (!importedRows.length) {
            showToast('No product rows found in the imported sheet', 'error');
            return;
        }

        inventoryModel.rows = importedRows;
        renderInventoryTable();
        saveInventorySnapshot(true);
        showToast(`Inventory imported: ${importedRows.length} products`, 'success');
    } catch (e) {
        showToast('Inventory import failed: ' + e.message, 'error');
    } finally {
        if (event && event.target) event.target.value = '';
    }
}

function toggleInventoryMailPanel() {
    const panel = document.getElementById('inventoryMailPanel');
    if (!panel) return;
    const willOpen = panel.style.display === 'none' || !panel.style.display;
    panel.style.display = willOpen ? 'block' : 'none';
    if (willOpen) {
        initInventoryMailSubjectHook();
        loadInventoryMailProfile();
        syncInventoryMailSubjectWithMonth();
    }
}

function loadInventoryMailProfile() {
    let profile = null;
    try {
        profile = JSON.parse(localStorage.getItem(INVENTORY_MAIL_PROFILE_KEY) || 'null');
    } catch (_) {
        profile = null;
    }
    if (!profile || typeof profile !== 'object') return;

    const map = [
        ['inventoryMailTo', 'to_email'],
        ['inventoryMailCc', 'cc_email'],
        ['inventoryMailFrom', 'sender_email'],
        ['inventoryMailHost', 'smtp_host'],
        ['inventoryMailPort', 'smtp_port'],
        ['inventoryMailSubject', 'subject'],
        ['inventoryMailNotes', 'notes']
    ];

    map.forEach(([id, key]) => {
        const el = document.getElementById(id);
        if (!el) return;
        if (profile[key] !== undefined && profile[key] !== null) {
            el.value = String(profile[key]);
        }
    });

    const remember = !!profile.remember_password;
    const rememberEl = document.getElementById('inventoryMailRememberPass');
    if (rememberEl) rememberEl.checked = remember;

    const passEl = document.getElementById('inventoryMailPass');
    if (passEl) {
        passEl.value = remember && profile.sender_password ? String(profile.sender_password) : '';
    }

    const avgModeEl = document.getElementById('inventoryMailAvgMode');
    if (avgModeEl) {
        const mode = String(profile.average_mode || 'monthly').toLowerCase();
        avgModeEl.value = mode === 'last7' ? 'last7' : 'monthly';
    }

    const onlyReorderEl = document.getElementById('inventoryMailOnlyReorder');
    if (onlyReorderEl) onlyReorderEl.checked = !!profile.only_reorder;

    const subjectEl = document.getElementById('inventoryMailSubject');
    if (subjectEl) {
        const autoText = getInventoryAutoSubjectText();
        subjectEl.dataset.autoSubject = String(subjectEl.value || '').trim() === autoText ? '1' : '0';
    }
}

function saveInventoryMailProfile() {
    const rememberPassword = !!document.getElementById('inventoryMailRememberPass')?.checked;
    const profile = {
        to_email: (document.getElementById('inventoryMailTo')?.value || '').trim(),
        cc_email: (document.getElementById('inventoryMailCc')?.value || '').trim(),
        sender_email: (document.getElementById('inventoryMailFrom')?.value || '').trim(),
        sender_password: rememberPassword ? (document.getElementById('inventoryMailPass')?.value || '').trim() : '',
        smtp_host: (document.getElementById('inventoryMailHost')?.value || 'smtp.gmail.com').trim(),
        smtp_port: Number(document.getElementById('inventoryMailPort')?.value || 587),
        subject: (document.getElementById('inventoryMailSubject')?.value || '').trim(),
        notes: (document.getElementById('inventoryMailNotes')?.value || '').trim(),
        average_mode: (document.getElementById('inventoryMailAvgMode')?.value || 'monthly').trim(),
        only_reorder: !!document.getElementById('inventoryMailOnlyReorder')?.checked,
        remember_password: rememberPassword
    };

    localStorage.setItem(INVENTORY_MAIL_PROFILE_KEY, JSON.stringify(profile));
    showToast('Mail profile saved', 'success');
}

function clearInventoryMailProfile() {
    localStorage.removeItem(INVENTORY_MAIL_PROFILE_KEY);
    const ids = [
        'inventoryMailTo',
        'inventoryMailCc',
        'inventoryMailFrom',
        'inventoryMailPass',
        'inventoryMailHost',
        'inventoryMailPort',
        'inventoryMailSubject',
        'inventoryMailNotes',
        'inventoryMailAvgMode'
    ];
    ids.forEach((id) => {
        const el = document.getElementById(id);
        if (el) el.value = '';
    });
    const hostEl = document.getElementById('inventoryMailHost');
    const portEl = document.getElementById('inventoryMailPort');
    if (hostEl) hostEl.value = 'smtp.gmail.com';
    if (portEl) portEl.value = '587';
    const avgModeEl = document.getElementById('inventoryMailAvgMode');
    if (avgModeEl) avgModeEl.value = 'monthly';
    const rememberEl = document.getElementById('inventoryMailRememberPass');
    if (rememberEl) rememberEl.checked = false;
    const onlyReorderEl = document.getElementById('inventoryMailOnlyReorder');
    if (onlyReorderEl) onlyReorderEl.checked = false;
    syncInventoryMailSubjectWithMonth(true);
    showToast('Mail profile cleared', 'info');
}

function getInventoryMailRows() {
    return (inventoryModel.rows || [])
        .map((r) => ({
            name: String(r.name || '').trim(),
            qty: Array.isArray(r.qty) ? r.qty.map((v) => Number(v || 0)) : [],
            min_stock: Number.isFinite(Number(r.min_stock)) && Number(r.min_stock) > 0 ? Number(r.min_stock) : 0,
            cost: Number.isFinite(Number(r.cost)) && Number(r.cost) > 0 ? Number(r.cost) : 0
        }))
        .filter((r) => r.name || r.qty.some((v) => Number(v || 0) !== 0) || Number(r.min_stock || 0) > 0 || Number(r.cost || 0) > 0);
}

async function previewInventoryPdf() {
    const rows = getInventoryMailRows();
    if (!rows.length) return showToast('No inventory rows to preview', 'error');

    const monthSelect = document.getElementById('inventoryMonthSelect');
    const monthText = monthSelect && monthSelect.options.length
        ? monthSelect.options[monthSelect.selectedIndex].text
        : String(inventoryModel.month || 'Month');

    const payload = {
        financial_year: getInventoryFinancialYear(),
        month: monthText,
        rows,
        average_mode: (document.getElementById('inventoryMailAvgMode')?.value || 'monthly').trim(),
        only_reorder: !!document.getElementById('inventoryMailOnlyReorder')?.checked
    };

    showToast('Generating PDF preview...', 'info');
    try {
        const res = await fetch('http://127.0.0.1:8000/report/inventory/pdf-preview', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });

        if (!res.ok) {
            const data = await res.json().catch(() => ({}));
            throw new Error(data.detail || `HTTP ${res.status}`);
        }

        const pdfBlob = await res.blob();
        const blobUrl = URL.createObjectURL(pdfBlob);
        const win = window.open(blobUrl, '_blank');
        if (!win) {
            const a = document.createElement('a');
            a.href = blobUrl;
            a.download = `Inventory_${monthText}_${getInventoryFinancialYear()}.pdf`.replace(/\s+/g, '_');
            document.body.appendChild(a);
            a.click();
            a.remove();
        }

        setTimeout(() => URL.revokeObjectURL(blobUrl), 30000);
    } catch (e) {
        showToast('Preview failed: ' + e.message, 'error');
    }
}

async function sendInventoryPdfMail() {
    const toEmail = (document.getElementById('inventoryMailTo')?.value || '').trim();
    const ccEmail = (document.getElementById('inventoryMailCc')?.value || '').trim();
    const senderEmail = (document.getElementById('inventoryMailFrom')?.value || '').trim();
    const senderPassword = (document.getElementById('inventoryMailPass')?.value || '').trim();
    const smtpHost = (document.getElementById('inventoryMailHost')?.value || 'smtp.gmail.com').trim();
    const smtpPort = Number(document.getElementById('inventoryMailPort')?.value || 587);
    const subject = (document.getElementById('inventoryMailSubject')?.value || '').trim();
    const notes = (document.getElementById('inventoryMailNotes')?.value || '').trim();
    const averageMode = (document.getElementById('inventoryMailAvgMode')?.value || 'monthly').trim();
    const onlyReorder = !!document.getElementById('inventoryMailOnlyReorder')?.checked;

    if (!toEmail) return showToast('Recipient email is required', 'error');
    if (!senderEmail) return showToast('Sender email is required', 'error');
    if (!senderPassword) return showToast('Sender app password is required', 'error');
    if (!smtpHost) return showToast('SMTP host is required', 'error');
    if (!Number.isFinite(smtpPort) || smtpPort <= 0) return showToast('SMTP port is invalid', 'error');

    const rows = getInventoryMailRows();
    if (!rows.length) return showToast('No inventory rows to send', 'error');

    const monthSelect = document.getElementById('inventoryMonthSelect');
    const monthText = monthSelect && monthSelect.options.length
        ? monthSelect.options[monthSelect.selectedIndex].text
        : String(inventoryModel.month || 'Month');

    const payload = {
        financial_year: getInventoryFinancialYear(),
        month: monthText,
        rows,
        average_mode: averageMode,
        only_reorder: onlyReorder,
        to_email: toEmail,
        cc_email: ccEmail,
        sender_email: senderEmail,
        sender_password: senderPassword,
        smtp_host: smtpHost,
        smtp_port: smtpPort,
        subject,
        notes
    };

    showToast('Preparing structured PDF and sending mail...', 'info');
    try {
        const res = await fetch('http://127.0.0.1:8000/report/inventory/email-pdf', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });

        const data = await res.json().catch(() => ({}));
        if (!res.ok) {
            throw new Error(data.detail || `HTTP ${res.status}`);
        }

        showToast(data.detail || 'Inventory PDF mailed successfully', 'success');
        const passInput = document.getElementById('inventoryMailPass');
        if (passInput) passInput.value = '';
        toggleInventoryMailPanel();
    } catch (e) {
        showToast('Mail send failed: ' + e.message, 'error');
    }
}

async function saveCashInHand(dateStr) {
    const input = document.getElementById(`cashInHand-${dateStr}`);
    if (!input) return;
    const val = parseFloat(input.value);
    if (Number.isNaN(val)) {
        showToast("Enter valid Cash In Hand", "error");
        return;
    }

    try {
        const res = await fetch("http://127.0.0.1:8000/cash/hand", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
                date: dateStr,
                cash_in_hand: val,
                admin_user: sessionStorage.getItem("username")
            })
        });
        const data = await res.json();
        if (data.status === "Saved") {
            showToast("Cash In Hand saved", "success");
            showDailySummary();
        } else {
            showToast("Save failed: " + data.detail, "error");
        }
    } catch (e) {
        showToast("Error: " + e, "error");
    }
}

async function resetCashInHand(dateStr) {
    if (!confirm(`Reset cash in hand for ${formatDateShort(dateStr)} to computed value?`)) return;

    try {
        const res = await fetch("http://127.0.0.1:8000/cash/hand/reset", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
                date: dateStr,
                admin_user: sessionStorage.getItem("username")
            })
        });
        const data = await res.json();
        if (data.status === "Reset") {
            showToast("Cash In Hand reset", "success");
            showDailySummary();
        } else {
            showToast("Reset failed: " + data.detail, "error");
        }
    } catch (e) {
        showToast("Error: " + e, "error");
    }
}

async function showModeReport(mode) {
    showView('modeReportView');
    document.getElementById("modeTitle").innerText = `${mode} Ledger`;
    const tbody = document.getElementById("modeReportBody");
    tbody.innerHTML = `<tr><td colspan="4" class="text-right">Loading...</td></tr>`;

    let data = [];
    try {
        const res = await fetchReport(`http://127.0.0.1:8000/report/mode/${mode}`);
        data = await res.json();
    } catch (e) {
        showToast("Mode report failed: " + e.message, "error");
        tbody.innerHTML = `<tr><td colspan="4" class="text-right">No Data</td></tr>`;
        return;
    }

    tbody.innerHTML = "";
    data.forEach(row => {
        tbody.innerHTML += `<tr><td>${formatDateShort(row.date)}</td><td>${row.party}</td><td>${row.type}</td><td class="text-right">${formatMoney(row.amount)}</td></tr>`;
    });
}

async function showExpenseReport() {
    showView('expenseReportView');
    const tbody = document.getElementById("expenseReportBody");
    tbody.innerHTML = `<tr><td colspan="4" class="text-right">Loading...</td></tr>`;

    let data = [];
    try {
        const res = await fetchReport("http://127.0.0.1:8000/report/type/Expense");
        data = await res.json();
    } catch (e) {
        showToast("Expense report failed: " + e.message, "error");
        tbody.innerHTML = `<tr><td colspan="4" class="text-right">No Data</td></tr>`;
        return;
    }

    let html = "";
    data.forEach(row => {
        html += `<tr><td>${formatDateShort(row.date)}</td><td>${row.party}</td><td>${row.mode}</td><td class="text-right">${formatMoney(row.amount)}</td></tr>`;
    });
    tbody.innerHTML = html || `<tr><td colspan="4" class="text-right">No Data</td></tr>`;
}

async function showOutstanding() {
    showView('outstandingView');
    let result = { data: [], total: 0, summary: {} };
    try {
        const res = await fetchReport("http://127.0.0.1:8000/report/outstanding");
        result = await res.json();
    } catch (e) {
        showToast("Outstanding report failed: " + e.message, "error");
        const tbody = document.getElementById("outstandingBody");
        if (tbody) tbody.innerHTML = `<tr><td colspan="5" class="text-right">No Data</td></tr>`;
        return;
    }
    outstandingData = result.data || [];
    const totalSpan = document.getElementById("totalOutstanding");
    if (totalSpan) totalSpan.textContent = `₹${formatMoney(result.total)}`;

    const summary = result.summary || {};
    const highCountEl = document.getElementById('outstandingHighCount');
    const criticalCountEl = document.getElementById('outstandingCriticalCount');
    const highAmountEl = document.getElementById('outstandingHighAmount');
    const criticalAmountEl = document.getElementById('outstandingCriticalAmount');
    if (highCountEl) highCountEl.textContent = String(summary.high_count || 0);
    if (criticalCountEl) criticalCountEl.textContent = String(summary.critical_count || 0);
    if (highAmountEl) highAmountEl.textContent = formatMoney(summary.high_amount || 0);
    if (criticalAmountEl) criticalAmountEl.textContent = formatMoney(summary.critical_amount || 0);

    const bell = document.getElementById('outstandingAlertBell');
    if (bell) {
        const count = Number(summary.critical_count || 0) + Number(summary.high_count || 0);
        bell.textContent = `Alerts: ${count}`;
    }

    const todayStr = new Date().toISOString().slice(0, 10);
    const lastAlertDate = sessionStorage.getItem('outstandingRiskAlertDate');
    if (lastAlertDate !== todayStr) {
        if (Number(summary.critical_count || 0) > 0) {
            showToast(`Alert: ${summary.critical_count} party(s) are 30+ days overdue`, 'error');
            sessionStorage.setItem('outstandingRiskAlertDate', todayStr);
        } else if (Number(summary.high_count || 0) > 0) {
            showToast(`Warning: ${summary.high_count} party(s) are 15+ days overdue`, 'warning');
            sessionStorage.setItem('outstandingRiskAlertDate', todayStr);
        }
    }

    renderOutstanding();
}

let outstandingData = [];

function renderOutstanding() {
    const tbody = document.getElementById("outstandingBody");
    if (!tbody) return;
    const sortEl = document.getElementById("outstandingSort");
    const sortValue = sortEl ? sortEl.value : 'risk-desc';

    const riskFilterEl = document.getElementById('outstandingRiskFilter');
    const riskFilter = riskFilterEl ? riskFilterEl.value : 'all';

    let filtered = (outstandingData || []).slice();
    if (riskFilter === 'high') {
        filtered = filtered.filter(x => String(x.risk_level || '') === 'high');
    } else if (riskFilter === 'critical') {
        filtered = filtered.filter(x => String(x.risk_level || '') === 'critical');
    } else if (riskFilter === 'no-receipt') {
        filtered = filtered.filter(x => !x.last_receipt_date);
    }

    if (outstandingCallListMode) {
        filtered.sort((a, b) => {
            const d = Number(b.days_unpaid || 0) - Number(a.days_unpaid || 0);
            if (d !== 0) return d;
            return Number(b.balance || 0) - Number(a.balance || 0);
        });
    }

    const sorted = filtered.slice().sort((a, b) => {
        if (sortValue === 'risk-desc') {
            const riskDiff = Number(b.risk_rank || 0) - Number(a.risk_rank || 0);
            if (riskDiff !== 0) return riskDiff;
            return Number(b.balance || 0) - Number(a.balance || 0);
        }
        if (sortValue === 'name-desc') {
            return String(b.party || '').localeCompare(String(a.party || ''), 'en', { sensitivity: 'base' });
        }
        if (sortValue === 'amount-desc') {
            return Number(b.balance || 0) - Number(a.balance || 0);
        }
        if (sortValue === 'amount-asc') {
            return Number(a.balance || 0) - Number(b.balance || 0);
        }
        if (sortValue === 'days-desc') {
            return Number(b.days_unpaid || 0) - Number(a.days_unpaid || 0);
        }
        if (sortValue === 'days-asc') {
            return Number(a.days_unpaid || 0) - Number(b.days_unpaid || 0);
        }
        return String(a.party || '').localeCompare(String(b.party || ''), 'en', { sensitivity: 'base' });
    });

    let html = '';
    sorted.forEach(row => {
        const riskLevel = row.risk_level || 'normal';
        const riskLabel = riskLevel === 'critical' ? '30+ Critical' : (riskLevel === 'high' ? '15+ Due' : 'Normal');
        const lastReceipt = row.last_receipt_date ? formatDateShort(row.last_receipt_date) : '-';
        html += `
            <tr data-party="${row.party || ''}">
                <td>${row.party}</td>
                <td><span class="outstanding-risk-chip ${riskLevel}">${riskLabel}</span></td>
                <td class="text-right">${Number(row.days_unpaid || 0)}</td>
                <td>${lastReceipt}</td>
                <td class="text-right">${formatMoney(row.balance)}</td>
            </tr>`;
    });

    tbody.innerHTML = html || `<tr><td colspan="5" class="text-right">No Data</td></tr>`;
    const panel = document.getElementById('outstandingCollectionPanel');
    if (panel && !sorted.length) panel.style.display = 'none';
    Array.from(tbody.querySelectorAll('tr[data-party]')).forEach(tr => {
        tr.style.cursor = 'pointer';
        tr.onclick = () => loadOutstandingPartyDetail(tr.getAttribute('data-party'));
    });
}

function toggleOutstandingCallListMode() {
    outstandingCallListMode = !outstandingCallListMode;
    const btn = document.getElementById('outstandingCallListBtn');
    if (btn) btn.textContent = `Call List: ${outstandingCallListMode ? 'On' : 'Off'}`;
    renderOutstanding();
}

function closeOutstandingCollectionPanel() {
    const panel = document.getElementById('outstandingCollectionPanel');
    if (panel) panel.style.display = 'none';
}

function showOutstandingAlerts() {
    const critical = Number(document.getElementById('outstandingCriticalCount')?.textContent || 0);
    const high = Number(document.getElementById('outstandingHighCount')?.textContent || 0);
    if (critical > 0) {
        showToast(`Critical alert: ${critical} party(s) crossed 30 days`, 'error');
    } else if (high > 0) {
        showToast(`Warning: ${high} party(s) crossed 15 days`, 'warning');
    } else {
        showToast('No overdue alerts for now', 'success');
    }
}

function snoozeOutstandingAlertsToday() {
    const todayStr = new Date().toISOString().slice(0, 10);
    sessionStorage.setItem('outstandingRiskAlertDate', todayStr);
    showToast('Outstanding alerts snoozed for today', 'info');
}

async function loadOutstandingPartyDetail(party) {
    if (!party) return;
    const panel = document.getElementById('outstandingCollectionPanel');
    const title = document.getElementById('collectionsPartyTitle');
    const meta = document.getElementById('collectionsPartyMeta');
    const action = document.getElementById('collectionsSuggestedAction');
    const body = document.getElementById('collectionsRecentBody');
    const statusChip = document.getElementById('collectionsPartyStatus');
    if (!panel || !title || !meta || !action || !body || !statusChip) return;

    panel.style.display = 'block';
    title.textContent = `${party} - Collections`; 
    meta.textContent = 'Loading...';
    action.textContent = '';
    body.innerHTML = `<tr><td colspan="6" class="text-right">Loading...</td></tr>`;

    try {
        const res = await fetchReport(`http://127.0.0.1:8000/report/outstanding/party?party=${encodeURIComponent(party)}`);
        const data = await res.json();
        const risk = Number(data.days_unpaid || 0) >= 30 ? 'critical' : (Number(data.days_unpaid || 0) >= 15 ? 'high' : 'normal');
        const label = risk === 'critical' ? '30+ Critical' : (risk === 'high' ? '15+ Due' : 'Normal');
        statusChip.className = `outstanding-risk-chip ${risk}`;
        statusChip.textContent = label;

        const lastReceipt = data.last_receipt_date ? formatDateShort(data.last_receipt_date) : '-';
        const hasClosingBalance = data.closing_balance !== null && data.closing_balance !== undefined && Number.isFinite(Number(data.closing_balance));
        const closingBal = hasClosingBalance ? Number(data.closing_balance) : Number(data.outstanding || 0);
        const closingLabel = closingBal < 0
            ? `${formatMoney(Math.abs(closingBal))} Cr`
            : `${formatMoney(closingBal)} Dr`;
        meta.textContent = `Outstanding: ${formatMoney(data.outstanding || 0)} | Days Unpaid: ${data.days_unpaid || 0} | Last Receipt: ${lastReceipt} | Ledger Closing: ${closingLabel}`;
        action.textContent = data.suggested_action || '';

        const recentLedger = Array.isArray(data.recent_ledger) ? data.recent_ledger : [];
        const legacyRecent = Array.isArray(data.recent_transactions) ? data.recent_transactions : [];
        const rows = (recentLedger.length ? recentLedger : legacyRecent).slice(0, 4);
        if (!rows.length) {
            body.innerHTML = `<tr><td colspan="6" class="text-right">No recent ledger transactions</td></tr>`;
            return;
        }
        let html = '';
        let reverseBalance = closingBal;
        rows.forEach(r => {
            const amount = Number(r.amount || 0);
            let bal = Number(r.balance);
            if (!Number.isFinite(bal)) {
                bal = reverseBalance;
                const tnorm = String(r.type || '').trim().toLowerCase();
                if (tnorm === 'sale') {
                    reverseBalance -= amount;
                } else if (tnorm === 'receipt' || tnorm === 'reciept' || tnorm === 'sale return') {
                    reverseBalance += amount;
                }
            }
            const balLabel = bal < 0 ? `${formatMoney(Math.abs(bal))} Cr` : `${formatMoney(bal)} Dr`;
            html += `<tr><td>${formatDateShort(r.date)}</td><td>${r.bill_no || ''}</td><td>${r.type || ''}</td><td>${r.mode || ''}</td><td class="text-right">${formatMoney(amount)}</td><td class="text-right">${balLabel}</td></tr>`;
        });
        body.innerHTML = html;
    } catch (e) {
        meta.textContent = 'Failed to load details';
        action.textContent = '';
        body.innerHTML = `<tr><td colspan="6" class="text-right">No Data</td></tr>`;
    }
}

async function showTrialBalance() {
    showView('trialBalanceView');
    const tbody = document.getElementById("trialBalanceBody");
    tbody.innerHTML = `<tr><td colspan="3" class="text-right">Loading...</td></tr>`;

    let data = [];
    try {
        const res = await fetchReport("http://127.0.0.1:8000/report/trial-balance");
        data = await res.json();
    } catch (e) {
        showToast("Trial Balance failed: " + e.message, "error");
        tbody.innerHTML = `<tr><td colspan="3" class="text-right">No Data</td></tr>`;
        return;
    }

    tbody.innerHTML = "";
    let tD = 0, tC = 0;
    data.forEach(row => {
        tD += row.debit; tC += row.credit;
        tbody.innerHTML += `<tr><td>${row.account}</td><td>${formatMoney(row.debit)}</td><td>${formatMoney(row.credit)}</td></tr>`;
    });
    tbody.innerHTML += `<tr style="font-weight:bold"><td>TOTAL</td><td>${formatMoney(tD)}</td><td>${formatMoney(tC)}</td></tr>`;
}

async function showPnL() {
    showView('pnlView');
    try {
        const res = await fetchReport("http://127.0.0.1:8000/report/pnl");
        const data = await res.json();
        document.getElementById("pnlSales").innerText = formatMoney(data.sales);
        document.getElementById("pnlExpenses").innerText = formatMoney(data.expenses);
        document.getElementById("pnlProfit").innerText = formatMoney(data.net_profit);
        document.getElementById("pnlProfit").style.color = data.net_profit >= 0 ? "var(--success)" : "var(--danger)";
    } catch (e) {
        showToast("P&L failed: " + e.message, "error");
    }
}

// Backup & Import
async function backupDB() {
    try {
        const res = await fetchWithTimeout(`http://127.0.0.1:8000/backup`, { method: "POST" }, 30000);
        const data = await res.json();
        if (data.status === "Backup Successful") {
            localStorage.setItem('lastBackupAt', new Date().toISOString());
            updateSystemStatus(true);
            if (data.warning) {
                showToast("Backup saved on server: " + data.path, "success");
                showToast(data.warning, "error");
            } else {
                showToast("Backup created: " + data.path, "success");
            }
        } else {
            showToast("Backup Failed: " + data.detail, "error");
        }
    } catch (e) { showToast("Backup failed: " + (e.message || e), "error"); }
}

async function runAutoBackupNow() {
    try {
        const res = await fetchWithTimeout(`http://127.0.0.1:8000/backup/auto`, { method: "POST" }, 30000);
        const data = await res.json();
        if (data.status === "Backup Successful") {
            localStorage.setItem('lastBackupAt', new Date().toISOString());
            updateSystemStatus(true);
            showToast("Auto backup created: " + data.path, "success");
        } else {
            showToast("Auto backup failed: " + data.detail, "error");
        }
    } catch (e) {
        showToast("Auto backup failed: " + (e.message || e), "error");
    }
}

async function openAutoBackupFolder() {
    try {
        const res = await electronAPI.invoke('folder:openAutoBackup');
        if (!res.success) {
            showToast('Open folder failed: ' + res.error, 'error');
        }
    } catch (e) {
        showToast('Open folder failed: ' + e.message, 'error');
    }
}

async function restoreDB() {
    const path = await electronAPI.invoke('dialog:openBackup');
    if (!path) return;
    if (!confirm("WARNING: This will overwrite the current database. Continue?")) return;
    try {
        const res = await fetchWithTimeout(`http://127.0.0.1:8000/restore?path=${encodeURIComponent(path)}`, { method: "POST" });
        const data = await res.json();
        if (data.status === "Restore Successful") {
            showToast("Restore Successful. Restarting...", "success");
            location.reload();
        } else showToast("Restore Failed: " + data.detail, "error");
    } catch (e) { showToast("Restore failed: " + (e.message || e), "error"); }
}

async function installServerMode() {
    try {
        const res = await electronAPI.invoke('server:install');
        if (res.success) {
            showToast('Server mode installed. Backend will start on boot.', 'success');
        } else {
            showToast('Install failed: ' + res.error, 'error');
        }
    } catch (e) {
        showToast('Install failed: ' + e.message, 'error');
    }
}

async function uninstallServerMode() {
    try {
        const res = await electronAPI.invoke('server:uninstall');
        if (res.success) {
            showToast('Server mode removed.', 'success');
        } else {
            showToast('Uninstall failed: ' + res.error, 'error');
        }
    } catch (e) {
        showToast('Uninstall failed: ' + e.message, 'error');
    }
}

async function restartBackend() {
    try {
        const res = await electronAPI.invoke('server:restart');
        if (res.success) {
            showToast('Backend restarted.', 'success');
            setTimeout(() => location.reload(), 500);
        } else {
            showToast('Restart failed: ' + res.error, 'error');
        }
    } catch (e) {
        showToast('Restart failed: ' + e.message, 'error');
    }
}

async function stopBackend() {
    try {
        const res = await electronAPI.invoke('server:stop');
        if (res.success) {
            showToast('Backend stopped.', 'success');
            updateSystemStatus(false);
            appStartTime = Date.now();
        } else {
            showToast('Stop failed: ' + res.error, 'error');
        }
    } catch (e) {
        showToast('Stop failed: ' + e.message, 'error');
    }
}


async function importData() {
    const fileInput = document.getElementById("importFile");
    if (!fileInput.files.length) return showToast("Select a file", "error");
    
    showToast("Importing... Please wait", "info");
    
    const formData = new FormData();
    formData.append("file", fileInput.files[0]);
    try {
        const res = await fetch("http://127.0.0.1:8000/import", { method: "POST", body: formData });
        const data = await res.json();
        
        if (data.status === "Imported" || data.status === "Import Successful") {
            // Show summary
            if (data.errors > 0) {
                showImportResultModal(data);
            } else {
                showToast(`✓ Import Complete: ${data.success} rows imported`, "success");
            }
            
            // Reload transactions
            loadTransactions();
            loadParties(true);
        } else {
            showToast("Import Failed: " + data.detail, "error");
        }
    } catch (e) { 
        showToast("Error: " + e, "error"); 
    }
}

function showImportResultModal(data) {
    const modal = document.createElement('div');
    modal.id = 'importResultModal';
    modal.style.cssText = 'position:fixed; top:0; left:0; width:100%; height:100%; background:rgba(0,0,0,0.5); z-index:3000; display:flex; justify-content:center; align-items:center;';
    
    const content = document.createElement('div');
    content.style.cssText = 'background:white; padding:30px; border-radius:12px; max-width:600px; max-height:80vh; overflow-y:auto; box-shadow:0 10px 25px rgba(0,0,0,0.2);';
    
    let failedRowsHTML = '';
    if (data.failed_rows && data.failed_rows.length > 0) {
        const displayRows = data.failed_rows.slice(0, 20);
        failedRowsHTML = '<div style="margin-top:15px; padding:15px; background:#fef2f2; border-radius:8px; max-height:300px; overflow-y:auto;">';
        failedRowsHTML += '<h4 style="margin:0 0 10px 0; color:#dc2626;">Failed Rows:</h4>';
        failedRowsHTML += '<ul style="margin:0; padding-left:20px; font-size:13px;">';
        displayRows.forEach(fail => {
            failedRowsHTML += `<li style="margin:5px 0;"><strong>Line ${fail.row}:</strong> ${fail.reason}</li>`;
        });
        failedRowsHTML += '</ul>';
        if (data.failed_rows.length > 20) {
            failedRowsHTML += `<p style="margin:10px 0 0 0; font-size:12px; color:#666;">... and ${data.failed_rows.length - 20} more failed rows</p>`;
        }
        failedRowsHTML += '</div>';
    }
    
    content.innerHTML = `
        <h3 style="margin:0 0 15px 0;">Import Results</h3>
        <div style="display:flex; gap:20px; margin-bottom:20px;">
            <div style="flex:1; padding:15px; background:#dcfce7; border-radius:8px; text-align:center;">
                <div style="font-size:32px; font-weight:bold; color:#16a34a;">${data.success || 0}</div>
                <div style="font-size:13px; color:#166534; margin-top:5px;">✓ Imported Successfully</div>
            </div>
            <div style="flex:1; padding:15px; background:#fee2e2; border-radius:8px; text-align:center;">
                <div style="font-size:32px; font-weight:bold; color:#dc2626;">${data.errors || 0}</div>
                <div style="font-size:13px; color:#991b1b; margin-top:5px;">✗ Failed to Import</div>
            </div>
        </div>
        ${failedRowsHTML}
        <button onclick="document.getElementById('importResultModal').remove()" style="width:100%; margin-top:20px; padding:12px; background:#4f46e5; color:white; border:none; border-radius:8px; cursor:pointer; font-size:14px; font-weight:600;">
            Close
        </button>
    `;
    
    modal.appendChild(content);
    document.body.appendChild(modal);
}

// Export to Excel
async function exportTableToExcel(tableId, filename = '', sheetName = 'Report') {
    try {
        const table = document.getElementById(tableId);
        if (!table) return showToast("Table not found to export", "error");

        const defaultName = filename ? filename + '.xlsx' : 'report.xlsx';

        // Ask Main Process for Save Path
        const filePath = await electronAPI.invoke('dialog:save', defaultName);
        if (!filePath) return; // User canceled

        if (ExcelJS) {
            // Sanitize sheet name
            const safeSheetName = (sheetName || "Report").replace(/[\/\\\?\*\[\]]/g, "_").substring(0, 31);
            const wb = new ExcelJS.Workbook();
            const ws = wb.addWorksheet(safeSheetName);

            const rows = Array.from(table.querySelectorAll('tr'))
                .map((row) => Array.from(row.querySelectorAll('th, td'))
                    .map((cell) => cell.innerText.trim()))
                .filter((row) => row.length > 0);
            rows.forEach((row) => ws.addRow(row));
            await wb.xlsx.writeFile(filePath);
        } else if (XLSX) {
            const wb = XLSX.utils.table_to_book(table, { sheet: (sheetName || 'Report').substring(0, 31) });
            XLSX.writeFile(wb, filePath);
        } else {
            return showToast("Excel export module missing (exceljs/xlsx)", "error");
        }

        showToast("Export Saved!", "success");
    } catch (e) {
        showToast("Export Failed: " + e.message, "error");
    }
}

function exportLedger() {
    const party = document.getElementById("reportParty").value;
    if (!party) return showToast("Select a party to export", "error");

    // Use party name for both filename and sheet name
    const cleanName = party.trim();
    exportTableToExcel('ledgerTable', cleanName, cleanName);
}

// Keyboard Shortcuts
document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
        const confirmModal = document.getElementById('confirmDeleteModal');
        const accessModal = document.getElementById('dbConfigAccessModal');
        const dbModal = document.getElementById('dbConfigModal');
        if (confirmModal && getComputedStyle(confirmModal).display !== 'none') {
            e.preventDefault();
            setModalVisible('confirmDeleteModal', false);
            pendingDeleteId = null;
            unlockUiAfterModal();
            return;
        }
        if (accessModal && getComputedStyle(accessModal).display !== 'none') {
            e.preventDefault();
            closeDbConfigAccessModal();
            return;
        }
        if (dbModal && getComputedStyle(dbModal).display !== 'none') {
            e.preventDefault();
            closeDbConfig();
            return;
        }
    }

    // Ctrl+S or Cmd+S: Save transaction edit
    if ((e.ctrlKey || e.metaKey) && e.key === 's') {
        e.preventDefault();
        if (editingTransactionId) {
            saveEntry();
        }
    }
    // Alt + N: New Entry (Focus Date)
    if (e.altKey && e.key === 'n') {
        e.preventDefault();
        showView('entryView');
        activateNav(document.querySelector('a[onclick*="entryView"]'));
        document.getElementById('newDate').focus();
    }
    // Alt + L: Ledger
    if (e.altKey && e.key === 'l') {
        e.preventDefault();
        showView('ledgerView');
        activateNav(document.querySelector('a[onclick*="ledgerView"]'));
        document.getElementById('reportParty').focus();
    }
    // Alt + D: Dashboard
    if (e.altKey && e.key === 'd') {
        e.preventDefault();
        showDashboard();
        activateNav(document.querySelector('a[onclick*="showDashboard"]'));
    }
    // Ctrl + K: Focus Search (Transaction Search)
    if ((e.ctrlKey || e.metaKey) && e.key === 'k') {
        e.preventDefault();
        const activeSearch = document.querySelector('.view-section.active input[placeholder*="Search"]');
        if (activeSearch) activeSearch.focus();
    }
});

// --- Authentication & Admin Logic ---

let currentUser = null;
let autoBackupTimer = null;
let isDeleting = false;
let pendingDeleteId = null;
window.isDeleting = false;

function setDeleteBusy(isBusy) {
    isDeleting = !!isBusy;
    window.isDeleting = !!isBusy;
}

function checkAuth() {
    currentUser = sessionStorage.getItem('username');
    currentRole = sessionStorage.getItem('role');
    const financialYear = sessionStorage.getItem('financialYear');

    const modal = document.getElementById('loginModal');
    const appContent = document.getElementById('appContent');

    if (!currentUser) {
        // Show Login
        setModalVisible('loginModal', true);
        appContent.style.filter = 'blur(5px)';
        appContent.style.pointerEvents = 'none';
        if (autoBackupTimer) {
            clearInterval(autoBackupTimer);
            autoBackupTimer = null;
        }
        loadFinancialYears();
        return;
    }

    // Hide Login & Show App
    setModalVisible('loginModal', false);
    appContent.style.filter = 'none';
    appContent.style.pointerEvents = 'auto';

    if (financialYear) {
        fetch('http://127.0.0.1:8000/financial-year/select', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ year: financialYear })
        });
    }

    startAutoBackup();
    updatePermissions();
    
    // Load essential data when already authenticated
    loadParties();
}

function startAutoBackup() {
    const startupBackupKey = 'startupAutoBackupDone';
    if (!sessionStorage.getItem(startupBackupKey)) {
        sessionStorage.setItem(startupBackupKey, '1');
        fetch('http://127.0.0.1:8000/backup/auto', { method: 'POST' }).catch(() => {});
    }

    if (autoBackupTimer) return;
    autoBackupTimer = setInterval(() => {
        fetch('http://127.0.0.1:8000/backup/auto', { method: 'POST' }).catch(() => {});
    }, 60 * 60 * 1000);
}

async function loadFinancialYears(retries = 5) {
    const select = document.getElementById('loginFinancialYear');
    if (!select) return;
    if (!select.innerHTML) {
        select.innerHTML = '<option value="">Loading financial years...</option>';
    }

    function financialYearForDate(dateObj) {
        const y = dateObj.getFullYear();
        const m = dateObj.getMonth() + 1;
        const start = m >= 4 ? y : y - 1;
        return `${start}-${start + 1}`;
    }

    function populateFallbackYears() {
        const now = new Date();
        const current = financialYearForDate(now);
        const prevStart = Number(current.split('-')[0]) - 1;
        const prev = `${prevStart}-${prevStart + 1}`;
        const nextStart = Number(current.split('-')[0]) + 1;
        const next = `${nextStart}-${nextStart + 1}`;
        const years = [next, current, prev];

        select.innerHTML = '';
        years.forEach(y => {
            const opt = document.createElement('option');
            opt.value = y;
            opt.textContent = y;
            select.appendChild(opt);
        });
        select.value = current;
    }

    try {
        const res = await fetchWithTimeout('http://127.0.0.1:8000/financial-years');
        if (!res.ok) {
            if (res.status === 404) {
                populateFallbackYears();
                showToast('Financial year API not found on backend. Using local defaults.', 'info');
                return;
            }
            throw new Error(`HTTP ${res.status}`);
        }
        const data = await res.json();
        const years = (data && data.years) ? data.years : [];
        select.innerHTML = '';
        if (!years.length) {
            populateFallbackYears();
            return;
        }
        years.forEach(y => {
            const opt = document.createElement('option');
            opt.value = y;
            opt.textContent = y;
            select.appendChild(opt);
        });
        if (data.selected) {
            select.value = data.selected;
        }
    } catch (e) {
        if (retries > 0) {
            setTimeout(() => loadFinancialYears(retries - 1), 1000);
        } else {
            populateFallbackYears();
            showToast('Could not load financial years. Using local defaults.', 'error');
        }
    }
}

async function handleLogin() {
    if (isLoggingIn) return;
    isLoggingIn = true;

    const user = document.getElementById('loginUser').value;
    const pass = document.getElementById('loginPass').value;
    const errorP = document.getElementById('loginError');
    const financialYear = document.getElementById('loginFinancialYear').value;
    const passInp = document.getElementById('loginPass');

    if (!user || !pass || !financialYear) {
        errorP.innerText = 'Select financial year and enter credentials';
        isLoggingIn = false;
        return;
    }

    try {
        try {
            const fyRes = await fetch('http://127.0.0.1:8000/financial-year/select', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ year: financialYear })
            });
            if (!fyRes.ok && fyRes.status !== 404) {
                throw new Error(`HTTP ${fyRes.status}`);
            }
        } catch (_) {
            // Backward compatibility: continue login for older backends without FY endpoint.
        }

        const res = await fetch('http://127.0.0.1:8000/login', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username: user, password: pass })
        });

        if (!res.ok) throw new Error('Incorrect password');

        const data = await res.json();
        sessionStorage.setItem('username', data.username);
        sessionStorage.setItem('role', data.role);
        sessionStorage.setItem('access_token', data.access_token);
        sessionStorage.setItem('financialYear', financialYear);

        checkAuth();
        showToast(`Hey ${data.username}, great to see you again! 😄`, "success");
        loadParties();
        
        // Load transactions AND show dashboard
        loadTransactions();
        showDashboard();

    } catch (e) {
        passInp.classList.add('shake');
        errorP.innerText = e.message || 'Incorrect password/ID';
        setTimeout(() => passInp.classList.remove('shake'), 500);
    } finally {
        isLoggingIn = false;
    }
}

function resetLogin() {
    const passEl = document.getElementById('loginPass');
    const userEl = document.getElementById('loginUser');
    if (passEl) passEl.value = '';
    if (userEl) userEl.focus();
}

function handleLogout() {
    sessionStorage.clear();
    if (autoBackupTimer) {
        clearInterval(autoBackupTimer);
        autoBackupTimer = null;
    }
    location.reload();
}

function updatePermissions() {
    // Admin Panel Visibility
    const adminPanel = document.getElementById('adminPanel');
    const renameCard = document.getElementById('renamePartyCard');

    if (currentRole === 'admin') {
        adminPanel.style.display = 'block';
        if (renameCard) renameCard.style.display = 'block';
        loadUsers();
        loadAuditLog();
        loadOpeningCashSeed();
    } else {
        adminPanel.style.display = 'none';
        if (renameCard) renameCard.style.display = 'none';
    }

    // Hide sensitive buttons for non-admins (e.g. Delete Party, specific edits)
    // For now, we mainly hide the admin panel. 
    // If there were "Edit" buttons in tables, we'd hide them here.
}

async function loadOpeningCashSeed() {
    try {
        const res = await fetchWithTimeout("http://127.0.0.1:8000/settings/opening-cash");
        const data = await res.json();
        const inp = document.getElementById("openingCashSeed");
        if (inp) inp.value = data.opening_cash ?? 0;
    } catch (e) {
        showToast("Error loading opening cash", "error");
    }
}

async function saveOpeningCashSeed() {
    const inp = document.getElementById("openingCashSeed");
    if (!inp) return;
    const val = parseFloat(inp.value);
    if (Number.isNaN(val)) {
        showToast("Enter valid opening cash", "error");
        return;
    }

    try {
        const res = await fetch("http://127.0.0.1:8000/settings/opening-cash", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
                amount: val,
                admin_user: sessionStorage.getItem("username")
            })
        });
        const data = await res.json();
        if (data.status === "Saved") {
            showToast("Opening cash saved", "success");
            showDailySummary();
        } else {
            showToast("Save failed: " + data.detail, "error");
        }
    } catch (e) {
        showToast("Error: " + e, "error");
    }
}

// --- Admin Functions ---

async function renameParty() {
    const oldName = document.getElementById('renameOldName').value.trim();
    const newName = document.getElementById('renameNewName').value.trim();
    const adminUser = sessionStorage.getItem('username');

    if (!oldName || !newName) return showToast("Enter both names", "error");
    if (!confirm(`Are you sure you want to rename "${oldName}" to "${newName}"? This will affect all historical records.`)) return;

    try {
        const res = await fetch('http://127.0.0.1:8000/party/rename', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ old_name: oldName, new_name: newName, admin_user: adminUser })
        });
        const data = await res.json();
        if (data.status === "Renamed Successfully") {
            showToast("Party Renamed!", "success");
            loadParties(true); // Refresh lists
            document.getElementById('renameOldName').value = '';
            document.getElementById('renameNewName').value = '';
        } else {
            showToast("Rename Failed: " + data.detail, "error");
        }
    } catch (e) { showToast(e.message, "error"); }
}

async function createUser() {
    const u = document.getElementById('newUsername').value;
    const p = document.getElementById('newPassword').value;
    const r = document.getElementById('newRole').value;

    if (!u || !p) return showToast("Enter username & password", "error");

    try {
        const token = sessionStorage.getItem('access_token');
        const res = await fetch('http://127.0.0.1:8000/users', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                ...(token ? { 'Authorization': `Bearer ${token}` } : {})
            },
            body: JSON.stringify({ username: u, password: p, role: r })
        });
        const data = await res.json();
        if (data.status === "User Created") {
            showToast("User Created", "success");
            loadUsers();
            document.getElementById('newUsername').value = '';
            document.getElementById('newPassword').value = '';
        } else {
            showToast(data.detail, "error");
        }
    } catch (e) { showToast(e.message, "error"); }
}

async function loadUsers() {
    const tbody = document.getElementById('usersTableBody');
    tbody.innerHTML = 'Loading...';
    try {
        const token = sessionStorage.getItem('access_token');
        const res = await fetchWithTimeout('http://127.0.0.1:8000/users', {
            headers: token ? { 'Authorization': `Bearer ${token}` } : {}
        });
        const users = await res.json();
        let rowsHtml = '';
        users.forEach(u => {
            rowsHtml += `
            <tr>
                <td>${u.username}</td>
                <td>${u.role}</td>
                <td>
                    ${u.username !== 'admin' ? `<button class="secondary" style="background:#EF4444; color:white; padding:4px 8px; font-size:12px;" onclick="deleteUser('${u.username}')">Delete</button>` : 'NA'}
                </td>
            </tr>`;
        });
        tbody.innerHTML = rowsHtml;
    } catch (e) { tbody.innerHTML = 'Error'; }
}

async function changeUserPassword() {
    const username = document.getElementById('pwdUsername').value.trim();
    const newPassword = document.getElementById('pwdNew').value;
    const adminUser = sessionStorage.getItem('username');

    if (!username || !newPassword) return showToast("Enter username & new password", "error");

    try {
        const token = sessionStorage.getItem('access_token');
        const res = await fetch('http://127.0.0.1:8000/users/password', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                ...(token ? { 'Authorization': `Bearer ${token}` } : {})
            },
            body: JSON.stringify({ username, new_password: newPassword })
        });
        const data = await res.json();
        if (data.status === "Password Updated") {
            showToast("Password updated", "success");
            document.getElementById('pwdUsername').value = '';
            document.getElementById('pwdNew').value = '';
        } else {
            showToast("Update failed: " + data.detail, "error");
        }
    } catch (e) {
        showToast("Error: " + e, "error");
    }
}

async function deleteUser(username) {
    if (!confirm("Are you sure?")) return;
    try {
        const token = sessionStorage.getItem('access_token');
        const res = await fetch(`http://127.0.0.1:8000/users/${username}`, {
            method: 'DELETE',
            headers: token ? { 'Authorization': `Bearer ${token}` } : {}
        });
        loadUsers();
    } catch (e) { showToast("Error deleting", "error"); }
}

function showAuditLogs() {
    showView('auditView');
}

let auditLogsData = [];
const AUDIT_ROW_HEIGHT = 32;
const AUDIT_OVERSCAN = 8;
let auditVirtualInitialized = false;

function initAuditVirtualScroll(containerId) {
    const container = document.getElementById(containerId);
    if (!container) return;
    if (!auditVirtualInitialized) {
        auditVirtualInitialized = true;
    }
    let ticking = false;
    container.addEventListener('scroll', () => {
        if (ticking) return;
        ticking = true;
        requestAnimationFrame(() => {
            renderAuditVirtual(containerId);
            ticking = false;
        });
    }, { passive: true });
}

function renderAuditVirtual(containerId, resetScroll = false) {
    const container = document.getElementById(containerId);
    const tbodyId = containerId === 'auditTableContainerView' ? 'auditTableBodyView' : 'auditTableBody';
    const tbody = document.getElementById(tbodyId);
    if (!container || !tbody) return;

    if (resetScroll) {
        container.scrollTop = 0;
    }

    const data = auditLogsData || [];
    const total = data.length;
    const containerHeight = container.clientHeight || 420;
    const scrollTop = container.scrollTop || 0;

    const measured = getMeasuredRowHeight(tbody, 'audit-spacer', auditMeasuredRowHeight);
    auditMeasuredRowHeight = measured || auditMeasuredRowHeight;
    const rowHeight = auditMeasuredRowHeight;
    const start = Math.max(0, Math.floor(scrollTop / rowHeight) - AUDIT_OVERSCAN);
    const visibleCount = Math.ceil(containerHeight / rowHeight) + AUDIT_OVERSCAN * 2;
    const end = Math.min(total, start + visibleCount);

    const topPad = start * rowHeight;
    const bottomPad = Math.max(0, (total - end) * rowHeight);
    const colspan = 4;

    let rowsHTML = '';
    if (topPad > 0) {
        rowsHTML += `<tr class="audit-spacer"><td colspan="${colspan}" style="height:${topPad}px; border:none; padding:0;"></td></tr>`;
    }

    for (let i = start; i < end; i += 1) {
        const l = data[i];
        rowsHTML += `
        <tr>
            <td>${new Date(l.timestamp).toLocaleString()}</td>
            <td>${l.username}</td>
            <td>${l.action}</td>
            <td>${l.details}</td>
        </tr>`;
    }

    if (bottomPad > 0) {
        rowsHTML += `<tr class="audit-spacer"><td colspan="${colspan}" style="height:${bottomPad}px; border:none; padding:0;"></td></tr>`;
    }

    tbody.innerHTML = rowsHTML;
}

async function loadAuditLog(targetBodyId = 'auditTableBody') {
    try {
        const token = sessionStorage.getItem('access_token');
        const res = await fetchWithTimeout('http://127.0.0.1:8000/audit', {
            headers: token ? { 'Authorization': `Bearer ${token}` } : {}
        });
        const logs = await res.json();
        auditLogsData = logs || [];
        const containerId = targetBodyId === 'auditTableBodyView' ? 'auditTableContainerView' : 'auditTableContainer';
        initAuditVirtualScroll(containerId);
        renderAuditVirtual(containerId, true);
    } catch (e) { showToast("Error: " + e.message, "error"); }
}

// Database Configuration Functions (continued)
async function loadDbConfig() {
    try {
        const localCfg = await loadClientConfig();
        document.getElementById('cfgApiBase').value = localCfg.api_base || apiBase;

        const token = sessionStorage.getItem('access_token');
        const res = await fetch('http://127.0.0.1:8000/config/database', {
            signal: AbortSignal.timeout(2000),
            headers: token ? { 'Authorization': `Bearer ${token}` } : {}
        });
        const config = await res.json();
        document.getElementById('cfgServer').value = config.server || '';
        document.getElementById('cfgDatabase').value = config.database || '';
        document.getElementById('cfgAuthType').value = config.auth_type || 'windows';
        document.getElementById('cfgUsername').value = config.username || '';
        document.getElementById('cfgBackupDir').value = config.backup_dir || '';
        toggleSqlAuth();
    } catch (e) {
        showToast("Backend not available, using defaults", "warning");
        // Load defaults when backend is not running
        const localCfg = await loadClientConfig();
        document.getElementById('cfgServer').value = 'localhost';
        document.getElementById('cfgDatabase').value = 'Finlogs';
        document.getElementById('cfgAuthType').value = 'windows';
        document.getElementById('cfgBackupDir').value = '';
        document.getElementById('cfgApiBase').value = localCfg.api_base || apiBase;
        toggleSqlAuth();
    }
}

async function testDbConnection() {
    const status = document.getElementById('dbConfigStatus');
    status.textContent = 'Testing connection...';
    status.style.color = '#FFA500';
    
    const server = document.getElementById('cfgServer').value.trim();
    const database = document.getElementById('cfgDatabase').value.trim();
    
    if (!server || !database) {
        status.textContent = '✗ Please enter server and database name';
        status.style.color = '#FF6B6B';
        return;
    }
    
    const config = {
        server: server,
        database: database,
        auth_type: document.getElementById('cfgAuthType').value,
        username: document.getElementById('cfgUsername').value.trim(),
        password: document.getElementById('cfgPassword').value,
        backup_dir: document.getElementById('cfgBackupDir').value.trim(),
        api_base: document.getElementById('cfgApiBase').value.trim()
    };
    
    try {
        const token = sessionStorage.getItem('access_token');
        const res = await fetch('http://127.0.0.1:8000/config/database/test', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                ...(token ? { 'Authorization': `Bearer ${token}` } : {})
            },
            body: JSON.stringify(config)
        });
        const result = await res.json();
        
        if (result.success) {
            status.textContent = '✓ Connection successful!';
            status.style.color = '#4CAF50';
        } else {
            status.textContent = '✗ Connection failed: ' + result.error;
            status.style.color = '#FF6B6B';
        }
    } catch (e) {
        status.textContent = '✗ Backend not running. Save config and restart app to test.';
        status.style.color = '#FFA500';
    }
}

async function saveDbConfig() {
    const status = document.getElementById('dbConfigStatus');
    
    const server = document.getElementById('cfgServer').value.trim();
    const database = document.getElementById('cfgDatabase').value.trim();
    
    if (!server || !database) {
        status.textContent = '✗ Please enter server and database name';
        status.style.color = '#FF6B6B';
        return;
    }
    
    const authType = document.getElementById('cfgAuthType').value;
    const username = document.getElementById('cfgUsername').value.trim();
    const password = document.getElementById('cfgPassword').value;
    
    if (authType === 'sql' && (!username || !password)) {
        status.textContent = '✗ Please enter SQL username and password';
        status.style.color = '#FF6B6B';
        return;
    }
    
    status.textContent = 'Saving configuration...';
    status.style.color = '#FFA500';
    
    const config = {
        server: server,
        database: database,
        auth_type: authType,
        username: username,
        password: password,
        backup_dir: document.getElementById('cfgBackupDir').value.trim(),
        api_base: document.getElementById('cfgApiBase').value.trim()
    };

    // Save client-side config first
    try {
        await saveClientConfig(config);
        if (config.api_base) {
            apiBase = normalizeApiBase(config.api_base);
        }
    } catch (e) {
        showToast("Failed to save configuration: " + e.message, "error");
    }
    
    try {
        const token = sessionStorage.getItem('access_token');
        const res = await fetch('http://127.0.0.1:8000/config/database', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                ...(token ? { 'Authorization': `Bearer ${token}` } : {})
            },
            body: JSON.stringify(config)
        });
        const result = await res.json();
        
        if (result.success) {
            status.textContent = '✓ Configuration saved! Please restart the application.';
            status.style.color = '#4CAF50';
            setTimeout(() => {
                closeDbConfig();
                alert('Database configuration saved. Please restart the application for changes to take effect.');
            }, 2000);
        } else {
            status.textContent = '✗ Save failed: ' + result.error;
            status.style.color = '#FF6B6B';
        }
    } catch (e) {
        // Backend not available - use Node.js fs to write config directly
        console.log('Backend not available, using direct file write');
        status.textContent = 'Backend offline. Saving directly to config file...';
        status.style.color = '#FFA500';
        
        try {
            await saveClientConfig(config);
            if (config.api_base) {
                apiBase = normalizeApiBase(config.api_base);
            }
            
            status.textContent = '✓ Configuration saved! Please restart the application.';
            status.style.color = '#4CAF50';
            setTimeout(() => {
                closeDbConfig();
                alert('Database configuration saved. Please close and restart the application.');
            }, 2000);
        } catch (fsError) {
            status.textContent = '✗ Failed to save: ' + fsError.message;
            status.style.color = '#FF6B6B';
        }    }
}

// Expose DB config functions globally for inline handlers
window.testDbConnection = testDbConnection;
window.saveDbConfig = saveDbConfig;
