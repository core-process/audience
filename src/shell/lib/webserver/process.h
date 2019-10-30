#pragma once

#include <string>
#include <memory>

struct WebserverContextData;
typedef std::shared_ptr<WebserverContextData> WebserverContext;

WebserverContext webserver_start(std::string address, unsigned short &port, std::string doc_root, int threads, std::function<void(WebserverContext, const std::wstring &)> on_message_handler);
void webserver_post_message(WebserverContext context, const std::wstring &message);
void webserver_stop(WebserverContext context);
