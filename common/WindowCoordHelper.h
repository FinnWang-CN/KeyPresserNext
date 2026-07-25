#pragma once

#include <windows.h>

namespace WindowCoordHelper {

/** WH_MOUSE_LL 物理像素 → 目标窗客户区坐标（Per-Monitor DPI 感知）。 */
bool ScreenToClientDpiAware(HWND target, int screen_x, int screen_y, int* client_x, int* client_y);

/** 判断 hwnd 是否属于 target 根窗口/所有权链。 */
bool BelongsToTarget(HWND target, HWND hwnd);

}  // namespace WindowCoordHelper
