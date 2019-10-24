#pragma once

#include <string>
#include <memory>

struct WebserverHandleData;
typedef std::shared_ptr<WebserverHandleData> WebserverHandle;

WebserverHandle webserver_start(std::string address, unsigned short &port, std::string doc_root, int threads);
void webserver_stop(WebserverHandle &handle);
