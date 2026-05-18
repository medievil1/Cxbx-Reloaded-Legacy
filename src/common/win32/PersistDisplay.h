#pragma once

#include <windows.h>
#include <cstdint>

namespace PersistDisplay {

constexpr uint32_t kMaxWidth = 3840;
constexpr uint32_t kMaxHeight = 2160;

bool Init(long long sessionID);
void Cleanup();
void Clear();
bool HasFrame();
bool StoreBgraFrame(const void* pixels, uint32_t width, uint32_t height, uint32_t strideBytes);
bool Paint(HDC hdc, const RECT& destRect);

}
