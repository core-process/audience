#include <comutil.h>
#include <mshtml.h>

#include "ie11_webview.h"
#include "../trace.h"

bool IEWebView::Create(HWND pwnd)
{
  parent_window = pwnd;

  HRESULT hr = 0;
  if (FAILED(hr = OleCreate(CLSID_WebBrowser, IID_IOleObject, OLERENDER_DRAW, 0, this, this, (LPVOID *)&webview)))
  {
    TRACEA(error, "failed to create COM instance of CLSID_WebBrowser, hr=0x" << std::hex << hr);
    return false;
  }

  if (FAILED(hr = webview->SetClientSite(this)))
  {
    TRACEA(error, "failed to set client site, hr=0x" << std::hex << hr);
    return false;
  }

  if (FAILED(hr = OleSetContainedObject(webview, TRUE)))
  {
    TRACEA(error, "failed to indicate embedding, hr=0x" << std::hex << hr);
    return false;
  }

  auto target_rect = GetWebViewTargetRect();

  if (FAILED(hr = webview->DoVerb(OLEIVERB_INPLACEACTIVATE, NULL, this, 0, parent_window, &target_rect)))
  {
    TRACEA(error, "failed to activate webview in place, hr=0x" << std::hex << hr);
    return false;
  }

  return true;
}

bool IEWebView::Navigate(std::wstring url)
{
  auto browser = GetWebviewBrowser2();
  if (browser != nullptr)
  {
    if (FAILED(browser->Navigate(bstr_t(url.c_str()), &variant_t(0x02u), 0, 0, 0)))
    {
      return false;
    }
    return true;
  }
  return false;
}

std::wstring IEWebView::GetDocumentTitle()
{
  auto browser = GetWebviewBrowser2();
  if (browser != nullptr)
  {
    IDispatch *ddoc = nullptr;
    if (FAILED(browser->get_Document(&ddoc)) || ddoc == nullptr)
    {
      return L"";
    }

    IHTMLDocument2 *doc = nullptr;
    if (FAILED(ddoc->QueryInterface(&doc)) || doc == nullptr)
    {
      return L"";
    }

    BSTR title = nullptr;
    if (FAILED(doc->get_title(&title)) || title == nullptr)
    {
      return L"";
    }

    auto result = std::wstring(title, SysStringLen(title));
    SysFreeString(title);

    return result;
  }

  return L"";
}

RECT IEWebView::GetWebViewTargetRect()
{
  RECT rect;
  if (!GetClientRect(parent_window, &rect))
  {
    TRACEA(error, "could not retrieve window client rect");
    return RECT{0, 0, 0, 0};
  }
  return RECT{0, 0, rect.right - rect.left, rect.bottom - rect.top};
}

void IEWebView::UpdateWebViewPosition()
{
  auto inplace = GetWebviewInplace();
  if (inplace != nullptr)
  {
    auto target_rect = GetWebViewTargetRect();
    inplace->SetObjectRects(&target_rect, &target_rect);
  }
}

IOleInPlaceObject *IEWebView::GetWebviewInplace()
{
  if (webview_inplace != nullptr)
  {
    return webview_inplace;
  }

  if (webview != nullptr)
  {
    if (FAILED(webview->QueryInterface(&webview_inplace)))
    {
      TRACEA(error, "could not retrieve IOleInPlaceObject interface");
    }
  }
  else
  {
    TRACEA(error, "webview not available");
  }

  return webview_inplace;
}

IWebBrowser2 *IEWebView::GetWebviewBrowser2()
{
  if (webview_browser2 != nullptr)
  {
    return webview_browser2;
  }

  if (webview != nullptr)
  {
    if (FAILED(webview->QueryInterface(&webview_browser2)))
    {
      TRACEA(error, "could not retrieve IWebBrowser2 interface");
    }
  }
  else
  {
    TRACEA(error, "webview not available");
  }

  return webview_browser2;
}

// ----- IUnknown -----

HRESULT STDMETHODCALLTYPE IEWebView::QueryInterface(REFIID riid, LPVOID *ppvObj)
{
  if (!ppvObj)
    return E_INVALIDARG;

  *ppvObj = NULL;

  if (riid == __uuidof(IUnknown) || riid == __uuidof(IOleClientSite))
  {
    *ppvObj = dynamic_cast<IOleClientSite *>(this);
  }
  else if (riid == __uuidof(IOleInPlaceSite) || riid == __uuidof(IOleWindow))
  {
    *ppvObj = dynamic_cast<IOleInPlaceSite *>(this);
  }
  else if (riid == __uuidof(IStorage))
  {
    *ppvObj = dynamic_cast<IStorage *>(this);
  }
  else
  {
    return E_NOINTERFACE;
  }

  AddRef();
  return NOERROR;
}

ULONG STDMETHODCALLTYPE IEWebView::AddRef(void)
{
  InterlockedIncrement(&ref_count);
  return ref_count;
}

ULONG STDMETHODCALLTYPE IEWebView::Release(void)
{
  // Decrement the object's internal counter.
  ULONG prev_ref_count = InterlockedDecrement(&ref_count);
  if (0 == ref_count)
  {
    delete this;
  }
  return prev_ref_count;
}

// ---------- IOleWindow ----------

HRESULT STDMETHODCALLTYPE IEWebView::GetWindow(
    __RPC__deref_out_opt HWND *phwnd)
{
  (*phwnd) = parent_window;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE IEWebView::ContextSensitiveHelp(
    BOOL fEnterMode)
{
  return E_NOTIMPL;
}

// ---------- IOleInPlaceSite ----------

HRESULT STDMETHODCALLTYPE IEWebView::CanInPlaceActivate(void)
{
  return S_OK;
}

HRESULT STDMETHODCALLTYPE IEWebView::OnInPlaceActivate(void)
{
  OleLockRunning(webview, TRUE, FALSE);

  auto inplace = GetWebviewInplace();
  if (inplace != nullptr)
  {
    auto target_rect = GetWebViewTargetRect();
    inplace->SetObjectRects(&target_rect, &target_rect);
  }

  return S_OK;
}

HRESULT STDMETHODCALLTYPE IEWebView::OnUIActivate(void)
{
  return S_OK;
}

HRESULT STDMETHODCALLTYPE IEWebView::GetWindowContext(
    __RPC__deref_out_opt IOleInPlaceFrame **ppFrame,
    __RPC__deref_out_opt IOleInPlaceUIWindow **ppDoc,
    __RPC__out LPRECT lprcPosRect,
    __RPC__out LPRECT lprcClipRect,
    __RPC__inout LPOLEINPLACEFRAMEINFO lpFrameInfo)
{
  if (!lprcPosRect || !lprcClipRect || !lpFrameInfo)
    return E_INVALIDARG;

  if (ppFrame)
  {
    (*ppFrame) = NULL;
  }
  if (ppDoc)
  {
    (*ppDoc) = NULL;
  }

  auto target_rect = GetWebViewTargetRect();
  (*lprcPosRect).left = target_rect.left;
  (*lprcPosRect).top = target_rect.top;
  (*lprcPosRect).right = target_rect.right;
  (*lprcPosRect).bottom = target_rect.bottom;

  *lprcClipRect = *lprcPosRect;

  lpFrameInfo->fMDIApp = false;
  lpFrameInfo->hwndFrame = parent_window;
  lpFrameInfo->haccel = NULL;
  lpFrameInfo->cAccelEntries = 0;

  return S_OK;
}

HRESULT STDMETHODCALLTYPE IEWebView::Scroll(
    SIZE scrollExtant)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::OnUIDeactivate(
    BOOL fUndoable)
{
  return S_OK;
}

HRESULT STDMETHODCALLTYPE IEWebView::OnInPlaceDeactivate(void)
{
  return S_OK;
}

HRESULT STDMETHODCALLTYPE IEWebView::DiscardUndoState(void)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::DeactivateAndUndo(void)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::OnPosRectChange(
    __RPC__in LPCRECT lprcPosRect)
{
  return E_NOTIMPL;
}

// ---------- IOleClientSite ----------

HRESULT STDMETHODCALLTYPE IEWebView::SaveObject(void)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::GetMoniker(
    DWORD dwAssign,
    DWORD dwWhichMoniker,
    __RPC__deref_out_opt IMoniker **ppmk)
{
  if ((dwAssign == OLEGETMONIKER_ONLYIFTHERE) &&
      (dwWhichMoniker == OLEWHICHMK_CONTAINER))
    return E_FAIL;

  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::GetContainer(
    __RPC__deref_out_opt IOleContainer **ppContainer)
{
  return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE IEWebView::ShowObject(void)
{
  return S_OK;
}

HRESULT STDMETHODCALLTYPE IEWebView::OnShowWindow(
    BOOL fShow)
{
  return S_OK;
}

HRESULT STDMETHODCALLTYPE IEWebView::RequestNewObjectLayout(void)
{
  return E_NOTIMPL;
}

// ----- IStorage -----

HRESULT STDMETHODCALLTYPE IEWebView::CreateStream(
    __RPC__in_string const OLECHAR *pwcsName,
    DWORD grfMode,
    DWORD reserved1,
    DWORD reserved2,
    __RPC__deref_out_opt IStream **ppstm)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::OpenStream(
    const OLECHAR *pwcsName,
    void *reserved1,
    DWORD grfMode,
    DWORD reserved2,
    IStream **ppstm)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::CreateStorage(
    __RPC__in_string const OLECHAR *pwcsName,
    DWORD grfMode,
    DWORD reserved1,
    DWORD reserved2,
    __RPC__deref_out_opt IStorage **ppstg)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::OpenStorage(
    __RPC__in_opt_string const OLECHAR *pwcsName,
    __RPC__in_opt IStorage *pstgPriority,
    DWORD grfMode,
    __RPC__deref_opt_in_opt SNB snbExclude,
    DWORD reserved,
    __RPC__deref_out_opt IStorage **ppstg)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::CopyTo(
    DWORD ciidExclude,
    const IID *rgiidExclude,
    __RPC__in_opt SNB snbExclude,
    IStorage *pstgDest)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::MoveElementTo(
    __RPC__in_string const OLECHAR *pwcsName,
    __RPC__in_opt IStorage *pstgDest,
    __RPC__in_string const OLECHAR *pwcsNewName,
    DWORD grfFlags)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::Commit(
    DWORD grfCommitFlags)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::Revert(void)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::EnumElements(
    DWORD reserved1,
    void *reserved2,
    DWORD reserved3,
    IEnumSTATSTG **ppenum)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::DestroyElement(
    __RPC__in_string const OLECHAR *pwcsName)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::RenameElement(
    __RPC__in_string const OLECHAR *pwcsOldName,
    __RPC__in_string const OLECHAR *pwcsNewName)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::SetElementTimes(
    __RPC__in_opt_string const OLECHAR *pwcsName,
    __RPC__in_opt const FILETIME *pctime,
    __RPC__in_opt const FILETIME *patime,
    __RPC__in_opt const FILETIME *pmtime)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::SetClass(
    __RPC__in REFCLSID clsid)
{
  return S_OK;
}

HRESULT STDMETHODCALLTYPE IEWebView::SetStateBits(
    DWORD grfStateBits,
    DWORD grfMask)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEWebView::Stat(
    __RPC__out STATSTG *pstatstg,
    DWORD grfStatFlag)
{
  return E_NOTIMPL;
}
