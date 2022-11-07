#pragma once

#ifndef WIN32_MEAN_AND_LEAN
#define WIN32_MEAN_AND_LEAN
#include <Windows.h>
#endif // !WIN32_MEAN_AND_LEAN

// Windows 7 does not have GetDpiForSystem
typedef UINT(WINAPI* GetDpiForSystemPtr) (void);
GetDpiForSystemPtr const get_system_dpi = [] {
  auto hMod = GetModuleHandleW(L"user32.dll");
  if (hMod) {
    return (GetDpiForSystemPtr)GetProcAddress(hMod, "GetDpiForSystem");
  }
  return (GetDpiForSystemPtr)nullptr;
}();

// input: desired font size on BallanceBug's screen
// window size: 1024x768; dpi: 119
int get_display_font_size(int display_height, float size) {
  return (int)std::round(display_height / (768.0f / 119) * size / ((get_system_dpi == nullptr) ? 96 : get_system_dpi()));
}

std::string ConvertWideToANSI(const std::wstring& wstr) {
  int count = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.length(), NULL, 0, NULL, NULL);
  std::string str(count, 0);
  WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &str[0], count, NULL, NULL);
  return str;
}

std::wstring ConvertUtf8ToWide(const std::string& str) {
  int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0);
  std::wstring wstr(count, 0);
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), &wstr[0], count);
  return wstr;
}

// ~std::unique_ptr does not give us any reference to the created raw pointer so we cannot set
// it to nullptr during destruction. We have to define our own custom smart pointer type.
template <typename T>
class my_ptr {
private:
  T **ptr_ptr = nullptr;

public:
  template <typename ... Args>
  my_ptr(T** ptr_ptr, const Args& ... args): ptr_ptr(ptr_ptr) {
    *ptr_ptr = new T(args...);
  }

  ~my_ptr() {
    delete *ptr_ptr;
    *ptr_ptr = nullptr;
  }
};
