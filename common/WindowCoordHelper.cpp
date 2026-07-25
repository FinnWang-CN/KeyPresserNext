#include "WindowCoordHelper.h"

namespace WindowCoordHelper {
namespace {

using PfnPhysicalToLogical = BOOL(WINAPI*)(HWND, LPPOINT);
using PfnLogicalToPhysical = BOOL(WINAPI*)(HWND, LPPOINT);

PfnPhysicalToLogical physicalToLogical() {
  static PfnPhysicalToLogical fn = reinterpret_cast<PfnPhysicalToLogical>(
      GetProcAddress(GetModuleHandleW(L"user32.dll"), "PhysicalToLogicalPointForPerMonitorDPI"));
  return fn;
}

PfnLogicalToPhysical logicalToPhysical() {
  static PfnLogicalToPhysical fn = reinterpret_cast<PfnLogicalToPhysical>(
      GetProcAddress(GetModuleHandleW(L"user32.dll"), "LogicalToPhysicalPointForPerMonitorDPI"));
  return fn;
}

}  // namespace

bool BelongsToTarget(HWND target, HWND hwnd) {
  if (!target || !hwnd || !IsWindow(target) || !IsWindow(hwnd)) {
    return false;
  }
  if (hwnd == target || IsChild(target, hwnd)) {
    return true;
  }
  HWND root = GetAncestor(hwnd, GA_ROOT);
  if (root == target) {
    return true;
  }
  HWND owner = GetWindow(hwnd, GW_OWNER);
  while (owner) {
    if (owner == target || GetAncestor(owner, GA_ROOT) == target) {
      return true;
    }
    owner = GetWindow(owner, GW_OWNER);
  }
  return false;
}

bool ScreenToClientDpiAware(HWND target, int screen_x, int screen_y, int* client_x, int* client_y) {
  if (!target || !IsWindow(target)) {
    return false;
  }

  POINT phys = {screen_x, screen_y};
  auto* physical_to_logical = physicalToLogical();
  auto* logical_to_physical = logicalToPhysical();

  bool on_target = BelongsToTarget(target, WindowFromPoint(phys));
  if (!on_target) {
    RECT wr = {};
    if (GetWindowRect(target, &wr)) {
      POINT tl = {wr.left, wr.top};
      POINT br = {wr.right, wr.bottom};
      if (logical_to_physical) {
        logical_to_physical(target, &tl);
        logical_to_physical(target, &br);
      }
      if (phys.x >= tl.x && phys.x < br.x && phys.y >= tl.y && phys.y < br.y) {
        on_target = true;
      }
    }
  }
  if (!on_target) {
    return false;
  }

  POINT pt = phys;
  if (physical_to_logical) {
    physical_to_logical(target, &pt);
  }
  if (!ScreenToClient(target, &pt)) {
    return false;
  }
  if (client_x) {
    *client_x = pt.x;
  }
  if (client_y) {
    *client_y = pt.y;
  }
  return true;
}

}  // namespace WindowCoordHelper
