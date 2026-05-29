# M-Finlogs Native — Qt UI Module

Premium Mac-grade minimalist UI built with C++ / Qt Widgets and heavily customized QSS.

## Architecture

```
qt-ui/
├── CMakeLists.txt      # Build configuration (Qt5/Qt6 compatible)
├── main.cpp            # Application entry point
├── mainwindow.h        # MainWindow class declaration
├── mainwindow.cpp      # MainWindow implementation (layout + table setup)
├── style.h             # Global QSS theme (drop-in via qApp->setStyleSheet)
└── README.md           # This file
```

## Design Principles

- **Typography:** Inter / Segoe UI at 10pt base, crisp rendering with high-DPI support
- **Colors:** Pure white backgrounds, deep charcoal text (#111827), no pure black
- **Borders:** All legacy 3D/sunken borders eliminated; flat 1px subtle borders only
- **Interactions:** Blue focus states (#3B82F6), soft selection highlights (#EFF6FF)

## Building

### Prerequisites
- CMake 3.16+
- Qt 5.15+ or Qt 6.x
- C++17 compiler

### Quick Start

```bash
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build .
./MFinlogsNative
```

### Windows (Visual Studio)

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH=C:/Qt/6.x/msvc2019_64
cmake --build . --config Release
```

## Applying the Theme

The stylesheet is applied globally in `main.cpp` via:

```cpp
qApp->setStyleSheet(MFinlogs::globalStyleSheet());
```

To use in an existing project, simply:
1. Include `style.h`
2. Call `qApp->setStyleSheet(MFinlogs::globalStyleSheet());` at startup
3. Set object names on your widgets to match the QSS selectors:
   - Sidebar: `setObjectName("sidebarNav")`
   - Form container: `setObjectName("inputFormContainer")`
   - Save button: `setObjectName("btnSaveEntry")`
   - Clear button: `setObjectName("btnClear")`
   - Search field: `setObjectName("searchBar")`
   - Page title: `setObjectName("pageTitle")`

## Column Alignment (Amount)

The Amount column is right-aligned using:

```cpp
// Header
QTableWidgetItem *amountHeader = table->horizontalHeaderItem(amountCol);
amountHeader->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

// Each cell
item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
```

## Customization

Edit `style.h` to adjust:
- Color palette (search for hex values)
- Border radius values
- Font sizes
- Spacing / padding
