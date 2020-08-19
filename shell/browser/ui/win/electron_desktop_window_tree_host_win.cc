// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/ui/win/electron_desktop_window_tree_host_win.h"

#include "base/win/windows_version.h"
#include "ui/base/win/hwnd_metrics.h"
#include "ui/base/win/shell.h"
#include "ui/display/win/dpi.h"
#include "ui/display/win/screen_win.h"

namespace electron {

ElectronDesktopWindowTreeHostWin::ElectronDesktopWindowTreeHostWin(
    NativeWindowViews* native_window_view,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura)
    : views::DesktopWindowTreeHostWin(native_window_view->widget(),
                                      desktop_native_widget_aura),
      native_window_view_(native_window_view) {}

ElectronDesktopWindowTreeHostWin::~ElectronDesktopWindowTreeHostWin() {}

bool ElectronDesktopWindowTreeHostWin::PreHandleMSG(UINT message,
                                                    WPARAM w_param,
                                                    LPARAM l_param,
                                                    LRESULT* result) {
  return native_window_view_->PreHandleMSG(message, w_param, l_param, result);
}

bool ElectronDesktopWindowTreeHostWin::ShouldPaintAsActive() const {
  // Tell Chromium to use system default behavior when rendering inactive
  // titlebar, otherwise it would render inactive titlebar as active under
  // some cases.
  // See also https://github.com/electron/electron/issues/24647.
  return false;
}

bool ElectronDesktopWindowTreeHostWin::HasNativeFrame() const {
  // Since we never use chromium's titlebar implementation, we can just say
  // that we use a native titlebar. This will disable the repaint locking when
  // DWM composition is disabled.
  return !ui::win::IsAeroGlassEnabled();
}

bool ElectronDesktopWindowTreeHostWin::GetDwmFrameInsetsInPixels(
    gfx::Insets* insets) const {
  if (IsMaximized() && !native_window_view_->has_frame()) {
    int caption_height =
        display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYSIZEFRAME) +
        display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYCAPTION);

    *insets = gfx::Insets(caption_height, 0, 0, 0);
    // The DWM API's expect values in pixels. We need to convert from DIP to
    // pixels here.
    *insets = insets->Scale(display::win::GetDPIScale());
    return true;
  }
  return false;
}

bool ElectronDesktopWindowTreeHostWin::GetClientAreaInsets(
    gfx::Insets* insets,
    HMONITOR monitor) const {
  if (IsMaximized() && !native_window_view_->has_frame()) {
    // Reduce the Windows non-client border size because we extend the border
    // into our client area in UpdateDWMFrame(). The top inset must be 0 or
    // else windows will draw a full native titlebar outside the client area.
    int frame_thickness = ui::GetFrameThickness(monitor);
    *insets = gfx::Insets(0, frame_thickness, frame_thickness, frame_thickness);
    return true;
  }
  return false;
}

}  // namespace electron
