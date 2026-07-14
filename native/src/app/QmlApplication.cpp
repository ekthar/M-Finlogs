#include "app/QmlApplication.h"

#include "app/AppContext.h"
#include "app/QmlBackend.h"
#include "app/SplashScreen.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QWindow>
#include <QtGlobal>

#include <cstring>
#include <exception>
#include <string>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#endif

namespace mfinlogs::app {

namespace {

// ─── Startup stage tracking ─────────────────────────────────────────────────

enum class StartupStage {
    NotStarted,
    QApplicationCreated,
    SplashShown,
    BackendCreated,
    QmlEngineCreated,
    QmlModuleLoaded,
    RootWindowCreated,
    RootWindowVisible,
    EventLoopStarted,
};

const char* startupStageName(StartupStage stage) {
    switch (stage) {
        case StartupStage::NotStarted:          return "NotStarted";
        case StartupStage::QApplicationCreated: return "QApplicationCreated";
        case StartupStage::SplashShown:         return "SplashShown";
        case StartupStage::BackendCreated:      return "BackendCreated";
        case StartupStage::QmlEngineCreated:    return "QmlEngineCreated";
        case StartupStage::QmlModuleLoaded:     return "QmlModuleLoaded";
        case StartupStage::RootWindowCreated:   return "RootWindowCreated";
        case StartupStage::RootWindowVisible:   return "RootWindowVisible";
        case StartupStage::EventLoopStarted:    return "EventLoopStarted";
    }
    return "Unknown";
}

// Global stage tracker - accessible from terminate handler.
static StartupStage g_currentStage = StartupStage::NotStarted;
static QStringList g_qmlErrors;

// ─── Font loading ───────────────────────────────────────────────────────────

void loadBundledFonts() {
    // Inter Tight + Space Mono ship in a sibling fonts/ directory. Mirror the
    // multi-path search used by the legacy widgets app so packaging is robust.
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList fontSearchPaths = {
        appDir + QStringLiteral("/fonts/"),
        appDir + QStringLiteral("/../fonts/"),
        appDir + QStringLiteral("/../../fonts/"),
        appDir + QStringLiteral("/../../../fonts/"),
    };
    for (const QString& fontDir : fontSearchPaths) {
        if (QDir(fontDir).exists()) {
            QFontDatabase::addApplicationFont(fontDir + QStringLiteral("InterTight-Regular.ttf"));
            QFontDatabase::addApplicationFont(fontDir + QStringLiteral("InterTight-Bold.ttf"));
            QFontDatabase::addApplicationFont(fontDir + QStringLiteral("SpaceMono-Regular.ttf"));
            QFontDatabase::addApplicationFont(fontDir + QStringLiteral("SpaceMono-Bold.ttf"));
            break;
        }
    }
}

// ─── Diagnostic file writer ─────────────────────────────────────────────────

QString writeStartupDiagnostic(const QString& detail,
                               const QQmlApplicationEngine* engine = nullptr) {
    const bool verificationRun = QCoreApplication::arguments().contains(QStringLiteral("--verify-qml"));
    const QString dataDir = verificationRun
        ? QCoreApplication::applicationDirPath()
        : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString logPath = QDir(dataDir.isEmpty() ? QDir::homePath() : dataDir)
        .filePath(QStringLiteral("startup-error.log"));

    QDir().mkpath(dataDir.isEmpty() ? QDir::homePath() : dataDir);

    QFile log(logPath);
    if (log.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream stream(&log);

        stream << QStringLiteral("=== M-Finlogs Startup Diagnostic ===") << Qt::endl;
        stream << Qt::endl;

        // Current startup stage
        stream << QStringLiteral("Startup Stage: ")
               << QString::fromLatin1(startupStageName(g_currentStage)) << Qt::endl;
        stream << Qt::endl;

        // Executable path
        stream << QStringLiteral("Executable Path: ")
               << QCoreApplication::applicationFilePath() << Qt::endl;

        // Application data path
        stream << QStringLiteral("App Data Path: ")
               << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) << Qt::endl;

        // Platform
        stream << QStringLiteral("Platform: ")
               << QGuiApplication::platformName() << Qt::endl;
        stream << Qt::endl;

        // Qt library paths
        stream << QStringLiteral("Qt Library Paths:") << Qt::endl;
        const QStringList libPaths = QCoreApplication::libraryPaths();
        for (const QString& path : libPaths) {
            stream << QStringLiteral("  - ") << path << Qt::endl;
        }
        stream << Qt::endl;

        // QML import paths (if engine is available)
        if (engine) {
            stream << QStringLiteral("QML Import Paths:") << Qt::endl;
            const QStringList importPaths = engine->importPathList();
            for (const QString& path : importPaths) {
                stream << QStringLiteral("  - ") << path << Qt::endl;
            }
            stream << Qt::endl;
        }

        // QML module path (embedded in binary at this resource prefix)
        stream << QStringLiteral("QML Module Path: :/qt/qml/MFinlogs") << Qt::endl;
        stream << Qt::endl;

        // Database mode
        stream << QStringLiteral("Database Mode: SQLite (local)") << Qt::endl;
        stream << Qt::endl;

        // QML errors collected during loading
        if (!g_qmlErrors.isEmpty()) {
            stream << QStringLiteral("QML Errors:") << Qt::endl;
            for (const QString& err : g_qmlErrors) {
                stream << QStringLiteral("  ") << err << Qt::endl;
            }
            stream << Qt::endl;
        }

        // Exception/failure detail
        stream << QStringLiteral("Failure Detail:") << Qt::endl;
        stream << detail << Qt::endl;
    }
    return logPath;
}

// ─── Startup failure reporter ───────────────────────────────────────────────

void reportStartupFailure(const QString& detail,
                          const QQmlApplicationEngine* engine = nullptr) {
    const QString logPath = writeStartupDiagnostic(detail, engine);
    QMessageBox::critical(nullptr,
        QStringLiteral("M-Finlogs could not start"),
        QStringLiteral("The application could not load its interface.\n\n%1\n\n"
                       "A diagnostic was saved to:\n%2")
            .arg(detail, QDir::toNativeSeparators(logPath)));
}

// ─── Terminate handler ──────────────────────────────────────────────────────

void startupTerminateHandler() {
    // After std::terminate the heap may be corrupt and locks may be held.
    // Keep this handler minimal: attempt a file write wrapped in try/catch,
    // then abort immediately. No QMessageBox (unsafe - requires event loop
    // and window creation in a potentially corrupt state).
    try {
        const QString detail = QStringLiteral(
            "std::terminate called during startup (possible uncaught exception or abort).");
        writeStartupDiagnostic(detail);
    } catch (...) {
        // Nothing we can safely do here.
    }
    std::abort();
}

#if defined(Q_OS_WIN)
// ─── SEH crash handler (raw WinAPI only) ───────────────────────────────────
//
// Access violations and other hardware exceptions are NOT C++ exceptions
// under MSVC's default /EHsc model: they cannot be caught by try/catch(...)
// or std::set_terminate. To diagnose crashes like 0xC0000005 during startup
// we install a SetUnhandledExceptionFilter that runs in the crashing thread
// *before* Windows terminates the process. The handler avoids the Qt/CRT
// heap (which may be corrupt) and uses only raw Win32 file APIs plus
// DbgHelp for a symbolized stack trace.

const char* exceptionCodeName(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:      return "EXCEPTION_ACCESS_VIOLATION";
        case EXCEPTION_STACK_OVERFLOW:        return "EXCEPTION_STACK_OVERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:   return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
        case EXCEPTION_PRIV_INSTRUCTION:      return "EXCEPTION_PRIV_INSTRUCTION";
        case EXCEPTION_HEAP_CORRUPTION:       return "EXCEPTION_HEAP_CORRUPTION";
        case 0xE06D7363:                      return "MSVC_CPP_EXCEPTION (0xE06D7363)";
        default:                              return "UNKNOWN";
    }
}

void appendRawLine(HANDLE file, const char* text) {
    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
}

void appendRawHex(HANDLE file, const char* label, DWORD64 value) {
    char buf[64];
    // sprintf_s is safe here: stack-based, no heap allocation.
    sprintf_s(buf, sizeof(buf), "%s0x%016llX\r\n", label, value);
    appendRawLine(file, buf);
}

// Resolves which loaded module (DLL/EXE) contains `address` and returns its
// base file name plus the offset within that module. This works without any
// PDB/symbol server - Release builds rarely ship symbols, so this is often
// the ONLY reliable way to tell which component (our exe vs. a Qt DLL vs.
// the CRT) actually crashed.
void appendModuleForAddress(HANDLE file, const char* label, DWORD64 address) {
    HMODULE hMod = nullptr;
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address), &hMod) && hMod != nullptr) {
        wchar_t modPath[MAX_PATH] = {0};
        DWORD len = GetModuleFileNameW(hMod, modPath, MAX_PATH);
        std::wstring name = (len > 0) ? std::wstring(modPath, len) : std::wstring(L"<unknown module>");
        const size_t slash = name.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            name = name.substr(slash + 1);
        }
        MODULEINFO modInfo = {};
        DWORD64 baseAddr = reinterpret_cast<DWORD64>(hMod);
        if (GetModuleInformation(GetCurrentProcess(), hMod, &modInfo, sizeof(modInfo))) {
            baseAddr = reinterpret_cast<DWORD64>(modInfo.lpBaseOfDll);
        }
        char nameBuf[MAX_PATH * 2];
        int written = WideCharToMultiByte(CP_UTF8, 0, name.c_str(), -1,
                                           nameBuf, sizeof(nameBuf), nullptr, nullptr);
        if (written <= 0) {
            nameBuf[0] = '\0';
        }
        char line[MAX_PATH * 2 + 128];
        sprintf_s(line, sizeof(line), "%s%s (module base 0x%016llX, offset 0x%llX)\r\n",
                  label, nameBuf, baseAddr, address - baseAddr);
        appendRawLine(file, line);
    } else {
        char line[128];
        sprintf_s(line, sizeof(line), "%s<address not in any loaded module>\r\n", label);
        appendRawLine(file, line);
    }
}

LONG WINAPI startupSehFilter(EXCEPTION_POINTERS* info) {
    // Build the crash log path next to the executable (writable in CI and
    // safe to inspect without needing AppData). Use raw Win32 calls only.
    wchar_t exePath[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring logPath(exePath);
    const size_t slash = logPath.find_last_of(L"\\/");
    logPath = (slash == std::wstring::npos) ? L"." : logPath.substr(0, slash);
    logPath += L"\\crash-error.log";

    HANDLE file = CreateFileW(logPath.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        appendRawLine(file, "=== M-Finlogs SEH Crash Handler ===\r\n");

        char stageBuf[128];
        sprintf_s(stageBuf, sizeof(stageBuf), "Startup Stage: %s\r\n",
                  startupStageName(g_currentStage));
        appendRawLine(file, stageBuf);

        if (info && info->ExceptionRecord) {
            const DWORD code = info->ExceptionRecord->ExceptionCode;
            char codeBuf[128];
            sprintf_s(codeBuf, sizeof(codeBuf), "Exception Code: 0x%08lX (%s)\r\n",
                      code, exceptionCodeName(code));
            appendRawLine(file, codeBuf);

            const DWORD64 faultAddr = reinterpret_cast<DWORD64>(info->ExceptionRecord->ExceptionAddress);
            appendRawHex(file, "Exception Address: ", faultAddr);
            appendModuleForAddress(file, "Crashing Module: ", faultAddr);

            if (code == EXCEPTION_ACCESS_VIOLATION &&
                info->ExceptionRecord->NumberParameters >= 2) {
                appendRawHex(file, "Access Violation Type (0=read,1=write): ",
                             info->ExceptionRecord->ExceptionInformation[0]);
                appendRawHex(file, "Faulting Address: ",
                             info->ExceptionRecord->ExceptionInformation[1]);
            }
        } else {
            appendRawLine(file, "Exception Code: <no exception record>\r\n");
        }

        // Best-effort symbolized stack trace via DbgHelp. Wrapped so a
        // failure here never prevents the rest of the log from being
        // written or the process from exiting.
        appendRawLine(file, "\r\nStack Trace:\r\n");
        HANDLE process = GetCurrentProcess();
        if (SymInitialize(process, nullptr, TRUE)) {
            SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

            CONTEXT contextCopy = *info->ContextRecord;
            STACKFRAME64 frame = {};
#if defined(_M_X64)
            DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
            frame.AddrPC.Offset = contextCopy.Rip;
            frame.AddrPC.Mode = AddrModeFlat;
            frame.AddrFrame.Offset = contextCopy.Rbp;
            frame.AddrFrame.Mode = AddrModeFlat;
            frame.AddrStack.Offset = contextCopy.Rsp;
            frame.AddrStack.Mode = AddrModeFlat;
#else
            DWORD machineType = IMAGE_FILE_MACHINE_I386;
            frame.AddrPC.Offset = contextCopy.Eip;
            frame.AddrPC.Mode = AddrModeFlat;
            frame.AddrFrame.Offset = contextCopy.Ebp;
            frame.AddrFrame.Mode = AddrModeFlat;
            frame.AddrStack.Offset = contextCopy.Esp;
            frame.AddrStack.Mode = AddrModeFlat;
#endif
            HANDLE thread = GetCurrentThread();
            for (int depth = 0; depth < 32; ++depth) {
                if (!StackWalk64(machineType, process, thread, &frame, &contextCopy,
                                  nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
                    break;
                }
                if (frame.AddrPC.Offset == 0) {
                    break;
                }
                char symBuffer[sizeof(SYMBOL_INFO) + 256] = {0};
                auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symBuffer);
                symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                symbol->MaxNameLen = 256;
                DWORD64 displacement = 0;
                char lineBuf[512];
                if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol)) {
                    sprintf_s(lineBuf, sizeof(lineBuf), "  #%d 0x%016llX %s+0x%llX\r\n",
                              depth, frame.AddrPC.Offset, symbol->Name, displacement);
                } else {
                    // Release builds usually ship no PDB, so SymFromAddr will
                    // fail for almost every frame. Fall back to naming the
                    // module (DLL/EXE) the address belongs to - this alone
                    // is enough to tell whether the crash is inside our
                    // code, Qt Quick, Qt Qml, or something else entirely.
                    sprintf_s(lineBuf, sizeof(lineBuf), "  #%d 0x%016llX <no symbol>\r\n",
                              depth, frame.AddrPC.Offset);
                }
                appendRawLine(file, lineBuf);
                appendModuleForAddress(file, "       module: ", frame.AddrPC.Offset);
            }
            SymCleanup(process);
        } else {
            appendRawLine(file, "  <SymInitialize failed - no stack trace available>\r\n");
        }

        CloseHandle(file);
    }

    // Let the default handler run afterward (process still terminates, but
    // now we have the log). EXCEPTION_CONTINUE_SEARCH lets Windows proceed
    // with normal unhandled-exception termination.
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif // Q_OS_WIN

} // namespace

int runQmlApplication(int argc, char** argv) {
    // Install terminate handler early to catch unexpected terminations.
    std::set_terminate(startupTerminateHandler);

#if defined(Q_OS_WIN)
    // Install a raw SEH handler so hardware exceptions (access violations,
    // stack overflows, etc.) during startup are logged with an address and
    // stack trace even though they cannot be caught by try/catch(...) or
    // std::set_terminate under MSVC's default /EHsc exception model.
    SetUnhandledExceptionFilter(startupSehFilter);
#endif

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("M-Finlogs"));
    app.setOrganizationName(QStringLiteral("Ekthar"));
    g_currentStage = StartupStage::QApplicationCreated;

    // The QWidget splash and the QML ApplicationWindow are separate top-level
    // windows. Keep the process alive while ownership moves from one to the
    // other; otherwise closing the splash can trigger quitOnLastWindowClosed.
    app.setQuitOnLastWindowClosed(false);

    // Set application icon (shows in taskbar + window chrome)
    const QIcon appIcon(QStringLiteral(":/icons/logo.svg"));
    app.setWindowIcon(appIcon);

    // Qt Quick Controls: use the Basic style so our custom QML theming fully
    // controls the look (Fusion/native styles ignore many custom properties).
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    loadBundledFonts();

    // Branded splash -- show early and keep visible until QML window renders.
    const bool verifyQml = QCoreApplication::arguments().contains(QStringLiteral("--verify-qml"));
    if (verifyQml) {
        qputenv("QSG_RHI_BACKEND", "null");
    }
    SplashScreen splash;
    if (!verifyQml) {
        splash.show();
        splash.runIndeterminate(2200);
        app.processEvents();
        // Process events several times to ensure splash paint completes.
        for (int i = 0; i < 5; ++i) {
            QThread::msleep(50);
            app.processEvents();
        }
    }
    g_currentStage = StartupStage::SplashShown;

    try {
        static AppContext context(QStringLiteral("Ekthar"), QStringLiteral("M-Finlogs"));
        static QmlBackend backend(context);
        g_currentStage = StartupStage::BackendCreated;

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        engine.rootContext()->setContextProperty(QStringLiteral("verifyQmlMode"), verifyQml);
        g_currentStage = StartupStage::QmlEngineCreated;

        QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
            [](const QList<QQmlError>& warnings) {
                for (const QQmlError& warning : warnings) {
                    g_qmlErrors.append(warning.toString());
                }
            });

        engine.loadFromModule(QStringLiteral("MFinlogs"), QStringLiteral("Main"));
        g_currentStage = StartupStage::QmlModuleLoaded;

        if (engine.rootObjects().isEmpty()) {
            const QString detail = g_qmlErrors.isEmpty()
                ? QStringLiteral("The MFinlogs QML module did not create a root window.")
                : g_qmlErrors.join(QLatin1Char('\n'));
            qCritical().noquote() << "M-Finlogs QML startup failure:" << detail;
            writeStartupDiagnostic(detail, &engine);
            if (!verifyQml) {
                splash.close();
                reportStartupFailure(detail, &engine);
            }
            return -1;
        }

        auto* mainWindow = qobject_cast<QWindow*>(engine.rootObjects().constFirst());
        if (!mainWindow) {
            const QString detail = QStringLiteral(
                "The QML root object is not a QWindow/ApplicationWindow. "
                "Root object type: %1")
                .arg(QString::fromLatin1(engine.rootObjects().constFirst()->metaObject()->className()));
            qCritical().noquote() << "M-Finlogs QML startup failure:" << detail;
            writeStartupDiagnostic(detail, &engine);
            if (!verifyQml) {
                splash.close();
                reportStartupFailure(detail, &engine);
            }
            return -1;
        }
        g_currentStage = StartupStage::RootWindowCreated;

        // In verify mode, we've confirmed the QML module loads and creates
        // a valid window. Exit without rendering (headless CI has no GPU).
        if (verifyQml) {
            app.processEvents();
            if (!g_qmlErrors.isEmpty()) {
                writeStartupDiagnostic(g_qmlErrors.join(QLatin1Char('\n')), &engine);
                return 2;
            }
            return 0;
        }

        mainWindow->setVisible(true);
        mainWindow->show();
        mainWindow->raise();
        mainWindow->requestActivate();
        g_currentStage = StartupStage::RootWindowVisible;

        // Keep the animated splash visible long enough to enjoy it, then
        // cross-fade out while the QML window is already rendered behind it.
        QTimer::singleShot(2000, &splash, [&splash, &app]() {
            splash.close();
            app.setQuitOnLastWindowClosed(true);
        });

        g_currentStage = StartupStage::EventLoopStarted;
        return app.exec();
    } catch (const std::exception& err) {
        splash.close();
        const QString detail = QString::fromUtf8(err.what());
        qCritical("Failed to start M-Finlogs: %s", err.what());
        if (!verifyQml) {
            reportStartupFailure(detail);
        } else {
            writeStartupDiagnostic(detail);
        }
        return 1;
    } catch (...) {
        splash.close();
        const QString detail = QStringLiteral("Unknown exception during startup.");
        qCritical("Failed to start M-Finlogs: unknown exception");
        if (!verifyQml) {
            reportStartupFailure(detail);
        } else {
            writeStartupDiagnostic(detail);
        }
        return 1;
    }
}

} // namespace mfinlogs::app
