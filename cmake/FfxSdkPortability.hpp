#pragma once

#include <FidelityFX/host/ffx_util.h>

#if !defined(_WIN32)
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <codecvt>
#include <cmath>
#include <cwchar>
#include <locale>
#include <string>

#ifndef _countof
#define _countof(array) (sizeof(array) / sizeof((array)[0]))
#endif

template <std::size_t N>
inline int ffx_sdk_wide_copy_s(wchar_t (&destination)[N],
                               const wchar_t *source) noexcept {
  if (source == nullptr) {
    destination[0] = L'\0';
    return EINVAL;
  }
  if (std::wcslen(source) >= N) {
    destination[0] = L'\0';
    return ERANGE;
  }
  return std::wcscpy(destination, source) == nullptr ? EINVAL : 0;
}

template <std::size_t N>
inline int ffx_sdk_wide_copy_s(wchar_t (&destination)[N], std::size_t capacity,
                               const wchar_t *source) noexcept {
  if (capacity > N) {
    destination[0] = L'\0';
    return EINVAL;
  }
  if (source == nullptr) {
    destination[0] = L'\0';
    return EINVAL;
  }
  if (std::wcslen(source) >= capacity) {
    destination[0] = L'\0';
    return ERANGE;
  }
  return std::wcscpy(destination, source) == nullptr ? EINVAL : 0;
}

inline int ffx_sdk_wide_copy_s(wchar_t *destination, std::size_t capacity,
                               const wchar_t *source) noexcept {
  if (destination == nullptr || capacity == 0U || source == nullptr) {
    if (destination != nullptr && capacity != 0U) destination[0] = L'\0';
    return EINVAL;
  }
  if (std::wcslen(source) >= capacity) {
    destination[0] = L'\0';
    return ERANGE;
  }
  return std::wcscpy(destination, source) == nullptr ? EINVAL : 0;
}

inline int ffx_sdk_narrow_copy_s(char *destination, std::size_t capacity,
                                 const char *source) noexcept {
  if (destination == nullptr || capacity == 0U || source == nullptr) {
    if (destination != nullptr && capacity != 0U) destination[0] = '\0';
    return EINVAL;
  }
  if (std::strlen(source) >= capacity) {
    destination[0] = '\0';
    return ERANGE;
  }
  return std::strcpy(destination, source) == nullptr ? EINVAL : 0;
}

#define wcscpy_s(...) ffx_sdk_wide_copy_s(__VA_ARGS__)
#define strcpy_s(destination, capacity, source) \
  ffx_sdk_narrow_copy_s(destination, capacity, source)
#define sprintf_s std::snprintf
using std::floor;
using std::log2;
#endif
