Keep it for now. Throw out in case current approach works fine.

# Fallback to Browser
- https://github.com/python/cpython/blob/3.8/Lib/webbrowser.py


# Serve Application

Proposed Schema: audience, e.g. audience://myapp/ or audience://myapp/main.js

## Unix QWebEngine
- https://doc.qt.io/qt-5/qtwebengine-webenginewidgets-simplebrowser-example.htmlgit

## Unix Webkit
- https://webkitgtk.org/reference/webkit2gtk/stable/WebKitWebContext.html#webkit-web-context-register-uri-scheme
- https://stackoverflow.com/a/27586984

## MacOS X Webkit
- https://developer.apple.com/documentation/webkit/wkwebviewconfiguration/2875766-seturlschemehandler?language=objc
- https://developer.apple.com/documentation/webkit/wkurlschemehandler?language=objc

## Windows Edge
- https://docs.microsoft.com/en-us/uwp/api/windows.web.ui.iwebviewcontrol.webresourcerequested
- https://docs.microsoft.com/en-us/uwp/api/windows.ui.xaml.controls.webview.navigatetolocalstreamuri
- https://code.msdn.microsoft.com/HTML-WebView-control-sample-56e773fa/sourcecode?fileId=99260&pathId=523934392

## Windows IE11
- https://stackoverflow.com/a/8325433
- https://code.google.com/archive/p/sumatrapdf/downloads
- https://www.codeproject.com/Articles/10368/An-ATL-control-for-hosting-and-customization-of-mu
- https://groups.google.com/forum/#!msg/microsoft.public.inetsdk.programming.mshtml_hosting/pQK0SoNviSU/s9SJohBJv3YJ


# Post Message: Native -> JS

## Unix Webkit
- https://webkitgtk.org/reference/webkit2gtk/stable/WebKitWebView.html#webkit-web-view-run-javascript

## MacOS X Webkit
- https://developer.apple.com/documentation/webkit/wkwebview/1415017-evaluatejavascript?language=objc

## Windows Edge
- https://docs.microsoft.com/en-us/uwp/api/windows.web.ui.iwebviewcontrol.invokescriptasync

## Windows IE11
- https://docs.microsoft.com/en-us/previous-versions//aa752116(v=vs.85)?redirectedfrom=MSDN
- https://docs.microsoft.com/en-us/previous-versions/aa752642(v%3Dvs.85)
- https://docs.microsoft.com/en-us/windows/win32/api/oaidl/nf-oaidl-idispatch-getidsofnames
- https://github.com/zserge/webview/blob/master/webview.h#L1389
- https://github.com/zserge/webview/blob/master/webview.h#L1416


# Post Message: JS -> Native

## Unix Webkit
- https://webkitgtk.org/reference/webkit2gtk/stable/WebKitUserContentManager.html#webkit-user-content-manager-register-script-message-handler
- https://webkitgtk.org/reference/webkit2gtk/stable/WebKitUserContentManager.html#WebKitUserContentManager-script-message-received

## MacOS X Webkit
- https://developer.apple.com/documentation/webkit/wkusercontentcontroller/1537172-addscriptmessagehandler?language=objc

## Windows Edge
- https://docs.microsoft.com/en-us/uwp/api/windows.web.ui.iwebviewcontrol.scriptnotify

## Windows IE11
- Implement IDocHostUIHandler
- https://docs.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa753256%28v%3dvs.85%29
- https://stackoverflow.com/a/36856486


# User Code

## Unix Webkit
- https://webkitgtk.org/reference/webkit2gtk/stable/webkit2gtk-4.0-WebKitUserContent.html#webkit-user-script-new
- https://webkitgtk.org/reference/webkit2gtk/stable/WebKitUserContentManager.html#webkit-user-content-manager-add-script

## MacOS X Webkit
- https://developer.apple.com/documentation/webkit/wkuserscript/1537750-initwithsource?language=objc
- https://developer.apple.com/documentation/webkit/wkusercontentcontroller/1537448-adduserscript?language=objc

## Windows Edge
- https://docs.microsoft.com/en-us/uwp/api/windows.web.ui.iwebviewcontrol2.addinitializescript

## Windows IE11
- https://stackoverflow.com/a/25860097
