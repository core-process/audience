#pragma once

#include <string>

bool _audience_inner_init();
AudienceHandle *_audience_inner_window_create(const std::wstring &title, const std::wstring &url);
void _audience_inner_window_destroy(AudienceHandle *handle);
void _audience_inner_loop();
