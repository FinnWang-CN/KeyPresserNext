#pragma once

#include <QPoint>
#include <QString>
#include <windows.h>

// Loads NextCore.dll and exposes Bind / Key / Mouse / FindPic / Capture.
class NextCoreAdapter {
 public:
  NextCoreAdapter();
  ~NextCoreAdapter();

  NextCoreAdapter(const NextCoreAdapter&) = delete;
  NextCoreAdapter& operator=(const NextCoreAdapter&) = delete;

  bool Initialize(const QString& dll_dir = QString());
  bool IsReady() const { return handle_ != nullptr; }
  QString Version() const;
  QString LastError() const { return last_error_; }

  bool BindWindow(HWND hwnd, const QString& display, const QString& mouse, const QString& keypad,
                  int mode);
  bool UnBindWindow();
  bool IsBound() const;
  HWND BindHwnd() const;

  bool KeyPress(int vk);
  bool KeyDown(int vk);
  bool KeyUp(int vk);

  bool MoveTo(int x, int y);
  bool LeftClick();
  bool LeftDoubleClick();
  bool LeftDown();
  bool LeftUp();
  bool RightClick();
  bool RightDown();
  bool RightUp();
  bool MiddleClick();
  bool WheelUp();
  bool WheelDown();

  // Returns true if found; out_pos is client coordinates of match.
  bool FindPic(int x1, int y1, int x2, int y2, const QString& image_path, double sim, QPoint* out_pos);
  bool Capture(int x1, int y1, int x2, int y2, const QString& file_path);
  bool GetClientSize(HWND hwnd, int* width, int* height);

 private:
  using CreateFn = void*(__stdcall*)();
  using DestroyFn = void(__stdcall*)(void*);
  using VerFn = const wchar_t*(__stdcall*)();
  using GetLastErrorFn = int(__stdcall*)(void*);
  using BindWindowFn = int(__stdcall*)(void*, intptr_t, const wchar_t*, const wchar_t*,
                                       const wchar_t*, int);
  using UnBindWindowFn = int(__stdcall*)(void*);
  using IsBindFn = int(__stdcall*)(void*);
  using GetBindWindowFn = intptr_t(__stdcall*)(void*);
  using KeyPressFn = int(__stdcall*)(void*, int);
  using KeyDownFn = int(__stdcall*)(void*, int);
  using KeyUpFn = int(__stdcall*)(void*, int);
  using MoveToFn = int(__stdcall*)(void*, int, int);
  using LeftClickFn = int(__stdcall*)(void*);
  using LeftDoubleClickFn = int(__stdcall*)(void*);
  using LeftDownFn = int(__stdcall*)(void*);
  using LeftUpFn = int(__stdcall*)(void*);
  using RightClickFn = int(__stdcall*)(void*);
  using RightDownFn = int(__stdcall*)(void*);
  using RightUpFn = int(__stdcall*)(void*);
  using MiddleClickFn = int(__stdcall*)(void*);
  using WheelUpFn = int(__stdcall*)(void*);
  using WheelDownFn = int(__stdcall*)(void*);
  using FindPicFn = int(__stdcall*)(void*, int, int, int, int, const wchar_t*, const wchar_t*,
                                    double, int, int*, int*);
  using CaptureFn = int(__stdcall*)(void*, int, int, int, int, const wchar_t*);
  using GetClientSizeFn = int(__stdcall*)(void*, intptr_t, int*, int*);

  template <typename T>
  bool Resolve(const char* name, T* out);

  void SetError(const QString& msg);

  HMODULE module_ = nullptr;
  void* handle_ = nullptr;
  QString last_error_;

  CreateFn create_ = nullptr;
  DestroyFn destroy_ = nullptr;
  VerFn ver_ = nullptr;
  GetLastErrorFn get_last_error_ = nullptr;
  BindWindowFn bind_window_ = nullptr;
  UnBindWindowFn unbind_window_ = nullptr;
  IsBindFn is_bind_ = nullptr;
  GetBindWindowFn get_bind_window_ = nullptr;
  KeyPressFn key_press_ = nullptr;
  KeyDownFn key_down_ = nullptr;
  KeyUpFn key_up_ = nullptr;
  MoveToFn move_to_ = nullptr;
  LeftClickFn left_click_ = nullptr;
  LeftDoubleClickFn left_double_click_ = nullptr;
  LeftDownFn left_down_ = nullptr;
  LeftUpFn left_up_ = nullptr;
  RightClickFn right_click_ = nullptr;
  RightDownFn right_down_ = nullptr;
  RightUpFn right_up_ = nullptr;
  MiddleClickFn middle_click_ = nullptr;
  WheelUpFn wheel_up_ = nullptr;
  WheelDownFn wheel_down_ = nullptr;
  FindPicFn find_pic_ = nullptr;
  CaptureFn capture_ = nullptr;
  GetClientSizeFn get_client_size_ = nullptr;
};
