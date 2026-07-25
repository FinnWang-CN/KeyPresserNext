QT += core gui widgets network
QT += core-private gui-private widgets-private

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = KeyPresserNextCommunity
TEMPLATE = app

CONFIG += c++17
CODECFORSRC = UTF-8
win32-msvc*: QMAKE_CXXFLAGS += /utf-8

!contains(QT_ARCH, x86_64) {
    error("KeyPresserNext Community requires a 64-bit Desktop Qt MSVC kit")
}

# Require administrator for hotkeys / hooks / injection against elevated targets
win32:CONFIG(release, debug|release) {
    QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'requireAdministrator\' uiAccess=\'false\'\"
}

DEFINES += UNICODE _UNICODE
LIBS += -luser32 -lgdi32 -luxtheme -ldwmapi -lshell32

# QWindowKit (frameless dark title bar)
QWK_INSTALL = $$PWD/third_party/qwindowkit-install
!exists($$QWK_INSTALL/share/QWindowKit/qmake/QWKWidgets.pri) {
    error("QWindowKit not found. Run scripts/build_qwindowkit.bat first.")
}
include($$QWK_INSTALL/share/QWindowKit/qmake/QWKWidgets.pri)

INCLUDEPATH += \
    $$PWD \
    $$PWD/common \
    $$PWD/engine \
    $$PWD/model \
    $$PWD/runtime \
    $$PWD/ui

SOURCES += \
    main.cpp \
    engine/Theme.cpp \
    engine/NextCoreAdapter.cpp \
    model/ActionRow.cpp \
    model/BulkRowImporter.cpp \
    model/SettingsStore.cpp \
    model/RecordConverter.cpp \
    runtime/RowExecutor.cpp \
    ui/KeyPresserNextWindow.cpp \
    ui/ActionRowWidget.cpp \
    ui/BulkAddRowsDialog.cpp \
    ui/DarkTitleBar.cpp \
    ui/HotkeySettingsDialog.cpp \
    common/KeyBinding.cpp \
    common/KeyCaptureEdit.cpp \
    common/ScreenshotButton.cpp \
    common/screenshotwidget.cpp \
    common/WindowSelectHelper.cpp \
    common/WindowCoordHelper.cpp \
    common/GlobalHotkeys.cpp \
    common/StopHotkeyHint.cpp \
    common/DarkWindowChrome.cpp \
    common/AppSingleInstance.cpp \
    common/FileAssociation.cpp \
    common/Changelog.cpp \
    common/LayoutMetrics.cpp

HEADERS += \
    appversion.h \
    engine/Theme.h \
    engine/NextCoreAdapter.h \
    model/ActionRow.h \
    model/BulkRowImporter.h \
    model/SettingsStore.h \
    model/RecordConverter.h \
    runtime/RowExecutor.h \
    ui/KeyPresserNextWindow.h \
    ui/ActionRowWidget.h \
    ui/BulkAddRowsDialog.h \
    ui/DarkTitleBar.h \
    ui/HotkeySettingsDialog.h \
    common/KeyBinding.h \
    common/KeyCaptureEdit.h \
    common/ScreenshotButton.h \
    common/screenshotwidget.h \
    common/WindowSelectHelper.h \
    common/WindowCoordHelper.h \
    common/GlobalHotkeys.h \
    common/StopHotkeyHint.h \
    common/DarkWindowChrome.h \
    common/AppSingleInstance.h \
    common/FileAssociation.h \
    common/Changelog.h \
    common/LayoutMetrics.h

RESOURCES += resources/res.qrc
RC_ICONS = resources/keypresser.ico

# Deploy NextCore runtime DLLs beside the executable.
NEXTCORE_SRC = $$shell_path($$PWD/engine/runtime/bin/NextCore.dll)
TOOLS_SRC = $$shell_path($$PWD/engine/runtime/bin/tools.dll)
NEXTBASE_SRC = $$shell_path($$PWD/engine/runtime/bin/NextBase.dll)
win32 {
    CONFIG(debug, debug|release) {
        OUT_DIR = $$OUT_PWD/debug
    } else {
        OUT_DIR = $$OUT_PWD/release
    }
    NEXTCORE_DST = $$shell_path($$OUT_DIR/NextCore.dll)
    TOOLS_DST = $$shell_path($$OUT_DIR/tools.dll)
    NEXTBASE_DST = $$shell_path($$OUT_DIR/NextBase.dll)
    QMAKE_POST_LINK = $$quote(cmd /c copy /Y \"$$NEXTCORE_SRC\" \"$$NEXTCORE_DST\" & copy /Y \"$$TOOLS_SRC\" \"$$TOOLS_DST\" & copy /Y \"$$NEXTBASE_SRC\" \"$$NEXTBASE_DST\")
}
