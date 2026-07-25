#include "NextCoreAdapter.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QtGlobal>

#include <string>

namespace {

std::wstring ToWide(const QString& s) {
  const auto utf16 = s.utf16();
  return std::wstring(reinterpret_cast<const wchar_t*>(utf16), static_cast<size_t>(s.size()));
}

// NextCore.dll export symbols use a fixed prefix; build names without that prefix in source.
QByteArray ExportName(const char* suffix) {
  QByteArray name;
  name.append(char(0x4F));
  name.append(char(0x70));
  name.append(suffix);
  return name;
}

}  // namespace

NextCoreAdapter::NextCoreAdapter() = default;

NextCoreAdapter::~NextCoreAdapter() {
  if (handle_ && destroy_) {
    destroy_(handle_);
    handle_ = nullptr;
  }
  if (module_) {
    FreeLibrary(module_);
    module_ = nullptr;
  }
}

template <typename T>
bool NextCoreAdapter::Resolve(const char* name, T* out) {
  FARPROC proc = GetProcAddress(module_, name);
  if (!proc) {
    SetError(QStringLiteral("NextCore.dll missing export: %1").arg(QLatin1String(name)));
    return false;
  }
  *out = reinterpret_cast<T>(proc);
  return true;
}

bool NextCoreAdapter::Initialize(const QString& dll_dir) {
  if (handle_) {
    return true;
  }

  QStringList search_dirs;
  if (!dll_dir.isEmpty()) {
    search_dirs << dll_dir;
  }
  search_dirs << QCoreApplication::applicationDirPath();
  search_dirs << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("engine/runtime/bin"));
  search_dirs << QDir(QCoreApplication::applicationDirPath())
                     .absoluteFilePath(QStringLiteral("../engine/runtime/bin"));
  search_dirs << QDir(QCoreApplication::applicationDirPath())
                     .absoluteFilePath(QStringLiteral("../../engine/runtime/bin"));

  QString dll_path;
  for (const QString& dir : search_dirs) {
    const QString candidate = QDir(dir).filePath(QStringLiteral("NextCore.dll"));
    if (QFileInfo::exists(candidate)) {
      dll_path = QDir::toNativeSeparators(candidate);
      break;
    }
  }
  if (dll_path.isEmpty()) {
    SetError(QStringLiteral("缺少 NextCore.dll"));
    return false;
  }

  // Ensure NextBase.dll / tools.dll in the same folder are found.
  SetDllDirectoryW(reinterpret_cast<LPCWSTR>(QFileInfo(dll_path).absolutePath().utf16()));

  module_ = LoadLibraryW(reinterpret_cast<LPCWSTR>(dll_path.utf16()));
  if (!module_) {
    SetError(QStringLiteral("无法加载 NextCore.dll (error %1)").arg(GetLastError()));
    return false;
  }

  const QByteArray n_create = ExportName("Create");
  const QByteArray n_destroy = ExportName("Destroy");
  const QByteArray n_ver = ExportName("Ver");
  const QByteArray n_get_last_error = ExportName("GetLastError");
  const QByteArray n_bind_window = ExportName("BindWindow");
  const QByteArray n_unbind_window = ExportName("UnBindWindow");
  const QByteArray n_is_bind = ExportName("IsBind");
  const QByteArray n_get_bind_window = ExportName("GetBindWindow");
  const QByteArray n_key_press = ExportName("KeyPress");
  const QByteArray n_key_down = ExportName("KeyDown");
  const QByteArray n_key_up = ExportName("KeyUp");
  const QByteArray n_move_to = ExportName("MoveTo");
  const QByteArray n_left_click = ExportName("LeftClick");
  const QByteArray n_left_double_click = ExportName("LeftDoubleClick");
  const QByteArray n_left_down = ExportName("LeftDown");
  const QByteArray n_left_up = ExportName("LeftUp");
  const QByteArray n_right_click = ExportName("RightClick");
  const QByteArray n_right_down = ExportName("RightDown");
  const QByteArray n_right_up = ExportName("RightUp");
  const QByteArray n_middle_click = ExportName("MiddleClick");
  const QByteArray n_wheel_up = ExportName("WheelUp");
  const QByteArray n_wheel_down = ExportName("WheelDown");
  const QByteArray n_find_pic = ExportName("FindPic");
  const QByteArray n_capture = ExportName("Capture");
  const QByteArray n_get_client_size = ExportName("GetClientSize");

  if (!Resolve(n_create.constData(), &create_) || !Resolve(n_destroy.constData(), &destroy_) ||
      !Resolve(n_ver.constData(), &ver_) || !Resolve(n_get_last_error.constData(), &get_last_error_) ||
      !Resolve(n_bind_window.constData(), &bind_window_) ||
      !Resolve(n_unbind_window.constData(), &unbind_window_) ||
      !Resolve(n_is_bind.constData(), &is_bind_) ||
      !Resolve(n_get_bind_window.constData(), &get_bind_window_) ||
      !Resolve(n_key_press.constData(), &key_press_) || !Resolve(n_key_down.constData(), &key_down_) ||
      !Resolve(n_key_up.constData(), &key_up_) || !Resolve(n_move_to.constData(), &move_to_) ||
      !Resolve(n_left_click.constData(), &left_click_) ||
      !Resolve(n_left_double_click.constData(), &left_double_click_) ||
      !Resolve(n_left_down.constData(), &left_down_) || !Resolve(n_left_up.constData(), &left_up_) ||
      !Resolve(n_right_click.constData(), &right_click_) ||
      !Resolve(n_right_down.constData(), &right_down_) ||
      !Resolve(n_right_up.constData(), &right_up_) ||
      !Resolve(n_middle_click.constData(), &middle_click_) ||
      !Resolve(n_wheel_up.constData(), &wheel_up_) || !Resolve(n_wheel_down.constData(), &wheel_down_) ||
      !Resolve(n_find_pic.constData(), &find_pic_) || !Resolve(n_capture.constData(), &capture_) ||
      !Resolve(n_get_client_size.constData(), &get_client_size_)) {
    FreeLibrary(module_);
    module_ = nullptr;
    return false;
  }

  handle_ = create_();
  if (!handle_) {
    SetError(QStringLiteral("Create failed"));
    return false;
  }
  last_error_.clear();
  return true;
}

QString NextCoreAdapter::Version() const {
  if (!ver_) {
    return QString();
  }
  return QString::fromWCharArray(ver_());
}

void NextCoreAdapter::SetError(const QString& msg) {
  last_error_ = msg;
  qWarning("%s", qPrintable(msg));
}

bool NextCoreAdapter::BindWindow(HWND hwnd, const QString& display, const QString& mouse,
                                 const QString& keypad, int mode) {
  if (!handle_ || !hwnd) {
    return false;
  }
  const std::wstring d = ToWide(display);
  const std::wstring m = ToWide(mouse);
  const std::wstring k = ToWide(keypad);
  const int ok = bind_window_(handle_, reinterpret_cast<intptr_t>(hwnd), d.c_str(), m.c_str(),
                              k.c_str(), mode);
  if (!ok) {
    SetError(QStringLiteral("BindWindow failed, lastError=%1").arg(get_last_error_(handle_)));
    return false;
  }
  last_error_.clear();
  return true;
}

bool NextCoreAdapter::UnBindWindow() {
  if (!handle_) {
    return true;
  }
  return unbind_window_(handle_) != 0;
}

bool NextCoreAdapter::IsBound() const {
  return handle_ && is_bind_ && is_bind_(handle_) != 0;
}

HWND NextCoreAdapter::BindHwnd() const {
  if (!handle_ || !get_bind_window_) {
    return nullptr;
  }
  return reinterpret_cast<HWND>(get_bind_window_(handle_));
}

bool NextCoreAdapter::KeyPress(int vk) { return handle_ && key_press_(handle_, vk) != 0; }
bool NextCoreAdapter::KeyDown(int vk) { return handle_ && key_down_(handle_, vk) != 0; }
bool NextCoreAdapter::KeyUp(int vk) { return handle_ && key_up_(handle_, vk) != 0; }
bool NextCoreAdapter::MoveTo(int x, int y) { return handle_ && move_to_(handle_, x, y) != 0; }
bool NextCoreAdapter::LeftClick() { return handle_ && left_click_(handle_) != 0; }
bool NextCoreAdapter::LeftDoubleClick() { return handle_ && left_double_click_(handle_) != 0; }
bool NextCoreAdapter::LeftDown() { return handle_ && left_down_(handle_) != 0; }
bool NextCoreAdapter::LeftUp() { return handle_ && left_up_(handle_) != 0; }
bool NextCoreAdapter::RightClick() { return handle_ && right_click_(handle_) != 0; }
bool NextCoreAdapter::RightDown() { return handle_ && right_down_(handle_) != 0; }
bool NextCoreAdapter::RightUp() { return handle_ && right_up_(handle_) != 0; }
bool NextCoreAdapter::MiddleClick() { return handle_ && middle_click_(handle_) != 0; }
bool NextCoreAdapter::WheelUp() { return handle_ && wheel_up_(handle_) != 0; }
bool NextCoreAdapter::WheelDown() { return handle_ && wheel_down_(handle_) != 0; }

bool NextCoreAdapter::FindPic(int x1, int y1, int x2, int y2, const QString& image_path, double sim,
                              QPoint* out_pos) {
  if (!handle_ || !out_pos) {
    return false;
  }
  const std::wstring path = ToWide(image_path);
  const std::wstring delta = ToWide(QStringLiteral("000000"));
  int x = 0;
  int y = 0;
  const int idx = find_pic_(handle_, x1, y1, x2, y2, path.c_str(), delta.c_str(), sim, 0, &x, &y);
  if (idx < 0) {
    return false;
  }
  out_pos->setX(x);
  out_pos->setY(y);
  return true;
}

bool NextCoreAdapter::Capture(int x1, int y1, int x2, int y2, const QString& file_path) {
  if (!handle_) {
    return false;
  }
  const std::wstring path = ToWide(file_path);
  return capture_(handle_, x1, y1, x2, y2, path.c_str()) != 0;
}

bool NextCoreAdapter::GetClientSize(HWND hwnd, int* width, int* height) {
  if (!handle_ || !hwnd || !width || !height) {
    return false;
  }
  return get_client_size_(handle_, reinterpret_cast<intptr_t>(hwnd), width, height) != 0;
}
