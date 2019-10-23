#pragma once

#include <string>
#include <memory>

struct WebserverHandle;

std::shared_ptr<WebserverHandle> webserver_start(std::string address, unsigned short &port, std::string doc_root, int threads);
void webserver_stop(std::shared_ptr<WebserverHandle> handle);
