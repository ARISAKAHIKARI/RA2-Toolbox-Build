#include "web_console.h"
#include <WebView2.h>
#include "web_console_assets.h"
#include <atomic>
#include <cstdint>
#include <cwchar>
#include <deque>
#include <utility>
#include <vector>

namespace consoleweb {
namespace {
constexpr size_t IncomingLimit = 64 * 1024;
constexpr size_t OutgoingLimit = 2 * 1024 * 1024;
constexpr size_t DeliveryLimit = 128;
constexpr UINT DeliverMessage = WM_APP + 0x34a;

// SDK interface identities are explicit because Microsoft's MIDL_INTERFACE
// annotations do not instantiate MinGW's __mingw_uuidof<T> templates.
template<class Interface> struct InterfaceIdentity;
#define CONSOLE_COM_ID(Type, A, B, C, ...) \
    template<> struct InterfaceIdentity<Type> { inline static constexpr GUID value{A, B, C, {__VA_ARGS__}}; };
CONSOLE_COM_ID(IUnknown, 0x00000000, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46)
CONSOLE_COM_ID(ICoreWebView2Settings3, 0xfdb5ab74, 0xaf33, 0x4854, 0x84, 0xf0, 0x0a, 0x63, 0x1d, 0xeb, 0x5e, 0xba)
CONSOLE_COM_ID(ICoreWebView2_4, 0x20d02d59, 0x6df2, 0x42dc, 0xbd, 0x06, 0xf9, 0x8a, 0x69, 0x4b, 0x13, 0x02)
CONSOLE_COM_ID(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, 0x4e8a3389, 0xc9d8, 0x4bd2, 0xb6, 0xb5, 0x12, 0x4f, 0xee, 0x6c, 0xc1, 0x4d)
CONSOLE_COM_ID(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, 0x6c4819f3, 0xc9b7, 0x4260, 0x81, 0x27, 0xc9, 0xf5, 0xbd, 0xe7, 0xf6, 0x8c)
CONSOLE_COM_ID(ICoreWebView2NavigationStartingEventHandler, 0x9adbe429, 0xf36d, 0x432b, 0x9d, 0xdc, 0xf8, 0x88, 0x1f, 0xbd, 0x76, 0xe3)
CONSOLE_COM_ID(ICoreWebView2NavigationCompletedEventHandler, 0xd33a35bf, 0x1c49, 0x4f98, 0x93, 0xab, 0x00, 0x6e, 0x05, 0x33, 0xfe, 0x1c)
CONSOLE_COM_ID(ICoreWebView2WebMessageReceivedEventHandler, 0x57213f19, 0x00e6, 0x49fa, 0x8e, 0x07, 0x89, 0x8e, 0xa0, 0x1e, 0xcb, 0xd2)
CONSOLE_COM_ID(ICoreWebView2NewWindowRequestedEventHandler, 0xd4c185fe, 0xc81c, 0x4989, 0x97, 0xaf, 0x2d, 0x3f, 0xa7, 0xab, 0x56, 0x51)
CONSOLE_COM_ID(ICoreWebView2PermissionRequestedEventHandler, 0x15e1c6a3, 0xc72a, 0x4df3, 0x91, 0xd7, 0xd0, 0x97, 0xfb, 0xec, 0x6b, 0xfd)
CONSOLE_COM_ID(ICoreWebView2ProcessFailedEventHandler, 0x79e0aea4, 0x990b, 0x42d9, 0xaa, 0x1d, 0x0f, 0xcc, 0x2e, 0x5b, 0xc7, 0xf1)
CONSOLE_COM_ID(ICoreWebView2DownloadStartingEventHandler, 0xefedc989, 0xc396, 0x41ca, 0x83, 0xf7, 0x07, 0xf8, 0x45, 0xa5, 0x57, 0x24)
#undef CONSOLE_COM_ID
template<class Interface> constexpr const GUID& interfaceId() { return InterfaceIdentity<Interface>::value; }


template<class Interface, class... Args>
class Callback final : public Interface {
    std::atomic<ULONG> references_{1};
    std::function<HRESULT(Args...)> invoke_;
public:
    explicit Callback(std::function<HRESULT(Args...)> invoke) : invoke_(std::move(invoke)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** value) override {
        if (!value) return E_POINTER;
        *value = nullptr;
        if (!IsEqualIID(iid, interfaceId<IUnknown>()) && !IsEqualIID(iid, interfaceId<Interface>())) return E_NOINTERFACE;
        *value = static_cast<Interface*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = --references_;
        if (!remaining) delete this;
        return remaining;
    }
    HRESULT STDMETHODCALLTYPE Invoke(Args... args) override { return invoke_(args...); }
};

template<class T> void release(T*& value) { if (value) { value->Release(); value = nullptr; } }

std::wstring widen(const char* bytes, size_t count) {
    if (!bytes || !count || count > OutgoingLimit) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, static_cast<int>(count), nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(static_cast<size_t>(size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, static_cast<int>(count), &result[0], size) != size) return {};
    return result;
}

std::string narrow(LPCWSTR text) {
    if (!text) return {};
    size_t length = 0;
    while (length <= IncomingLimit && text[length]) ++length;
    if (length > IncomingLimit) return {};
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
    if (size <= 0 || static_cast<size_t>(size) > IncomingLimit) return {};
    std::string result(static_cast<size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, static_cast<int>(length), &result[0], size, nullptr, nullptr) != size) return {};
    return result;
}

bool localDocument(LPCWSTR uri) { return uri && lstrcmpW(uri, L"about:blank") == 0; }

// NavigateToString's requested URL can be a data URL while its completed
// document and WebMessage source are about:blank. Authorize the exact bytes
// issued by this Host; never accept an arbitrary data: navigation.
std::wstring embeddedRequestUri(const char* data,size_t count) {
    static constexpr wchar_t alphabet[]=L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if(!data||!count||count>OutgoingLimit)return {};
    std::wstring result=L"data:text/html;charset=utf-8;base64,";result.reserve(result.size()+((count+2)/3)*4);
    for(size_t at=0;at<count;at+=3){const unsigned a=static_cast<unsigned char>(data[at]),b=at+1<count?static_cast<unsigned char>(data[at+1]):0,c=at+2<count?static_cast<unsigned char>(data[at+2]):0;
        result+=alphabet[a>>2];result+=alphabet[((a&3)<<4)|(b>>4)];result+=at+1<count?alphabet[((b&15)<<2)|(c>>6)]:L'=';result+=at+2<count?alphabet[c&63]:L'=';
    }return result;
}
const wchar_t* sourceKind(LPCWSTR uri){return !uri?L"null":localDocument(uri)?L"about:blank":wcsncmp(uri,L"data:text/html;charset=utf-8;base64,",34)==0?L"embedded-data":L"other";}
void navigationTrace(const wchar_t* stage, unsigned long long id, LPCWSTR uri,
                     HRESULT successResult, BOOL success, HRESULT errorResult,
                     COREWEBVIEW2_WEB_ERROR_STATUS webError, HRESULT sourceResult) {
    wchar_t detail[512]{};
    if(wcsncmp(stage,L"NavigationStarting",18)==0)
        std::swprintf(detail,512,L"RA2.Console %ls id=%llu UriHR=0x%08lX admitted=%d NavigationIdHR=0x%08lX source-kind=%ls\n",stage,id,static_cast<unsigned long>(successResult),int(success),static_cast<unsigned long>(sourceResult),sourceKind(uri));
    else std::swprintf(detail,512,L"RA2.Console %ls id=%llu IsSuccessHR=0x%08lX success=%d WebErrorHR=0x%08lX WebErrorStatus=%d SourceHR=0x%08lX source-kind=%ls\n",
        stage,id,static_cast<unsigned long>(successResult),int(success),static_cast<unsigned long>(errorResult),int(webError),static_cast<unsigned long>(sourceResult),sourceKind(uri));
#ifdef RA2_CONSOLE_NAVIGATION_TRACE
    RA2_CONSOLE_NAVIGATION_TRACE(detail);
#else
    OutputDebugStringW(detail);
#endif
}
std::wstring navigationFailure(UINT64 id,HRESULT successResult,BOOL success,HRESULT errorResult,COREWEBVIEW2_WEB_ERROR_STATUS webError,HRESULT sourceResult,LPCWSTR uri){
    wchar_t detail[512]{};std::swprintf(detail,512,L"控制台页面加载失败。导航 %llu；IsSuccess HRESULT=0x%08lX，success=%d；WebErrorStatus=%d（HRESULT=0x%08lX）；来源=%ls（HRESULT=0x%08lX）。请重试；持续失败时保留这些错误信息。",static_cast<unsigned long long>(id),static_cast<unsigned long>(successResult),int(success),int(webError),static_cast<unsigned long>(errorResult),sourceKind(uri),static_cast<unsigned long>(sourceResult));return detail;
}

std::wstring moduleDirectory() {
    std::vector<wchar_t> path(32768);
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return {};
    std::wstring result(path.data(), length);
    const auto separator = result.find_last_of(L"\\/");
    return separator == std::wstring::npos ? std::wstring{} : result.substr(0, separator);
}

bool ensureDirectory(const std::wstring& path) {
    if (CreateDirectoryW(path.c_str(), nullptr)) return true;
    if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring userDataDirectory() {
    const DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (!count || count > 32768) return {};
    std::vector<wchar_t> value(count);
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", value.data(), count);
    if (!length || length >= count) return {};
    std::wstring root(value.data(), length);
    root += L"\\RA2Toolbox";
    if (!ensureDirectory(root)) return {};
    root += L"\\WebView2";
    return ensureDirectory(root) ? root : std::wstring{};
}
}

struct Host::State : std::enable_shared_from_this<Host::State> {
    HWND parent = nullptr, dispatcher = nullptr;
    DWORD owner = GetCurrentThreadId();
    RECT bounds{};
    bool closed = false, pageReady = false, deliveryPosted = false, failed = false;
    bool documentPending = false;UINT64 documentNavigationId = 0;std::wstring documentRequestUri;
    HMODULE loader = nullptr;
    ICoreWebView2Environment* environment = nullptr;
    ICoreWebView2Controller* controller = nullptr;
    ICoreWebView2* view = nullptr;
    ICoreWebView2_4* downloads = nullptr;
    MessageHandler onMessage;
    ErrorHandler onError;
    std::wstring dataDirectory;
    struct Delivery { std::string message; std::wstring error; };
    std::deque<Delivery> deliveries;
    std::deque<std::string> earlyMessages;
    EventRegistrationToken navigation{}, frameNavigation{}, completed{}, message{}, newWindow{}, permission{}, process{}, download{};
    bool hasNavigation = false, hasFrameNavigation = false, hasCompleted = false, hasMessage = false;
    bool hasNewWindow = false, hasPermission = false, hasProcess = false, hasDownload = false;

    ~State() {
        dispose();
        // Pending environment/controller callbacks also retain State. Thus the
        // loader remains mapped until the final asynchronous callback releases.
        if (loader) FreeLibrary(loader);
    }
    bool usable() const { return !closed && GetCurrentThreadId() == owner && parent && IsWindow(parent); }

    static LRESULT CALLBACK dispatchProc(HWND window, UINT event, WPARAM a, LPARAM b) {
        auto* state = reinterpret_cast<State*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (event == WM_NCCREATE) {
            state = static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(b)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        }
        if (event == DeliverMessage && state) {
            // A callback may close/destroy Host. Keep this dispatch alive until
            // it returns without retaining the object after window teardown.
            const auto keepAlive = state->shared_from_this();
            state->drain();
            return 0;
        }
        if (event == WM_NCDESTROY) SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        return DefWindowProcW(window, event, a, b);
    }

    bool createDispatcher() {
        WNDCLASSW wc{};
        wc.lpfnWndProc = dispatchProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"RA2.Console.AsyncDispatch";
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        dispatcher = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, this);
        return dispatcher != nullptr;
    }

    void wake() {
        if (!closed && dispatcher && !deliveryPosted && !deliveries.empty())
            deliveryPosted = PostMessageW(dispatcher, DeliverMessage, 0, 0) != FALSE;
    }
    void error(const wchar_t* text) {
        if (closed || failed) return;
        failed = true;
        pageReady = false;documentPending=false;documentRequestUri.clear();
        earlyMessages.clear();
        if (deliveries.size() >= DeliveryLimit) deliveries.clear();
        deliveries.push_back({{}, text});
        wake();
    }
    void incoming(std::string json) {
        if (!usable() || failed || json.empty()) return;
        if (!pageReady) {
            if(!documentPending||!documentNavigationId)return;
            if (earlyMessages.size() >= DeliveryLimit) { error(L"控制台初始化消息过多，请重新打开工具箱。"); return; }
            earlyMessages.push_back(std::move(json));
            return;
        }
        if (deliveries.size() >= DeliveryLimit) { error(L"控制台操作队列已满，请重新打开工具箱。"); return; }
        deliveries.push_back({std::move(json), {}});
        wake();
    }
    void drain() {
        deliveryPosted = false;
        // A burst of settings changes cannot monopolize the shell's STA.
        for (unsigned count = 0; usable() && !deliveries.empty() && count < 32; ++count) {
            Delivery next = std::move(deliveries.front());
            deliveries.pop_front();
            if (!next.error.empty()) { if (onError) onError(next.error); }
            else if (pageReady && onMessage) onMessage(next.message);
        }
        wake();
    }

    void dispose() {
        if (closed) return;
        closed = true;
        pageReady = false;
        deliveries.clear(); earlyMessages.clear();
        if (view) {
            if (hasNavigation) view->remove_NavigationStarting(navigation);
            if (hasFrameNavigation) view->remove_FrameNavigationStarting(frameNavigation);
            if (hasCompleted) view->remove_NavigationCompleted(completed);
            if (hasMessage) view->remove_WebMessageReceived(message);
            if (hasNewWindow) view->remove_NewWindowRequested(newWindow);
            if (hasPermission) view->remove_PermissionRequested(permission);
            if (hasProcess) view->remove_ProcessFailed(process);
        }
        if (downloads && hasDownload) downloads->remove_DownloadStarting(download);
        if (controller) controller->Close();
        release(downloads); release(view); release(controller); release(environment);
        if (dispatcher) { const auto window = dispatcher; dispatcher = nullptr; DestroyWindow(window); }
        parent = nullptr;
        onMessage = {}; onError = {};
    }

    HRESULT initializeController(HRESULT result, ICoreWebView2Controller* created) {
        if (!usable()) return S_OK;
        if (FAILED(result) || !created) { error(L"无法创建控制台界面，请更新 Microsoft Edge WebView2 运行环境后重试。"); return S_OK; }
        controller = created; controller->AddRef();
        if (FAILED(controller->get_CoreWebView2(&view)) || !view) { error(L"无法连接控制台界面，请重新打开工具箱。"); return S_OK; }
        ICoreWebView2Settings* settings = nullptr;
        if (FAILED(view->get_Settings(&settings)) || !settings) { error(L"无法初始化控制台设置，请重新打开工具箱。"); return S_OK; }
        const HRESULT settingsResults[] = {
            settings->put_IsScriptEnabled(TRUE), settings->put_IsWebMessageEnabled(TRUE),
            settings->put_AreHostObjectsAllowed(FALSE), settings->put_AreDefaultScriptDialogsEnabled(FALSE),
            settings->put_AreDefaultContextMenusEnabled(FALSE), settings->put_AreDevToolsEnabled(FALSE),
            settings->put_IsStatusBarEnabled(FALSE), settings->put_IsZoomControlEnabled(FALSE)
        };
        ICoreWebView2Settings3* keyboardSettings = nullptr;
        const bool keyboardReady = SUCCEEDED(settings->QueryInterface(interfaceId<ICoreWebView2Settings3>(), reinterpret_cast<void**>(&keyboardSettings))) &&
            keyboardSettings && SUCCEEDED(keyboardSettings->put_AreBrowserAcceleratorKeysEnabled(FALSE));
        release(keyboardSettings);
        settings->Release();
        for (const auto status : settingsResults) if (FAILED(status)) { error(L"WebView2 运行环境不兼容，请更新后重试。"); return S_OK; }
        if (!keyboardReady) { error(L"WebView2 运行环境版本过旧，请更新后重试。"); return S_OK; }
        auto self = shared_from_this();
        auto* navigationHandler = new Callback<ICoreWebView2NavigationStartingEventHandler, ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs*>(
            [self](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* event) {
                LPWSTR uri=nullptr;UINT64 id=0;
                const HRESULT uriResult=event?event->get_Uri(&uri):E_POINTER;
                const HRESULT idResult=event?event->get_NavigationId(&id):E_POINTER;
                const bool admitted=self->usable()&&!self->failed&&self->documentPending&&SUCCEEDED(uriResult)&&SUCCEEDED(idResult)&&id&&uri&&
                    uri==self->documentRequestUri&&(!self->documentNavigationId||id==self->documentNavigationId);
                if(event)event->put_Cancel(admitted?FALSE:TRUE);
                if(admitted)self->documentNavigationId=id;
                navigationTrace(admitted?L"NavigationStarting-authorized":L"NavigationStarting-denied",id,uri,uriResult,admitted,S_OK,COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN,idResult);
                CoTaskMemFree(uri);return S_OK;
            });
        hasNavigation = SUCCEEDED(view->add_NavigationStarting(navigationHandler, &navigation));
        navigationHandler->Release();
        auto* frameHandler = new Callback<ICoreWebView2NavigationStartingEventHandler, ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs*>(
            [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* event){if(event)event->put_Cancel(TRUE);return S_OK;});
        hasFrameNavigation=SUCCEEDED(view->add_FrameNavigationStarting(frameHandler,&frameNavigation));frameHandler->Release();
        auto* completedHandler = new Callback<ICoreWebView2NavigationCompletedEventHandler, ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*>(
            [self](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* event) {
                if (!self->usable()) return S_OK;
                BOOL success = FALSE; LPWSTR uri = nullptr;UINT64 id=0;
                const HRESULT successResult=event?event->get_IsSuccess(&success):E_POINTER;
                COREWEBVIEW2_WEB_ERROR_STATUS webError=COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
                const HRESULT errorResult=event?event->get_WebErrorStatus(&webError):E_POINTER;
                if(event)event->get_NavigationId(&id);
                const HRESULT sourceResult=sender?sender->get_Source(&uri):E_POINTER;
                navigationTrace(L"NavigationCompleted",id,uri,successResult,success,errorResult,webError,sourceResult);
                // Initial blank and denied/unrelated navigations can finish after
                // our document starts. Their cancellation cannot poison this Host.
                if(self->failed||!self->documentPending||!self->documentNavigationId||id!=self->documentNavigationId){CoTaskMemFree(uri);return S_OK;}
                if (!event || FAILED(successResult) || !success || !sender || FAILED(sourceResult) || !localDocument(uri)) {
                    const auto detail=navigationFailure(id,successResult,success,errorResult,webError,sourceResult,uri);
                    CoTaskMemFree(uri); self->error(detail.c_str()); return S_OK;
                }
                CoTaskMemFree(uri);self->documentPending=false;self->documentRequestUri.clear();
                self->pageReady = true;
                auto waiting = std::move(self->earlyMessages);
                self->earlyMessages.clear();
                for (auto& json : waiting) self->incoming(std::move(json));
                return S_OK;
            });
        hasCompleted = SUCCEEDED(view->add_NavigationCompleted(completedHandler, &completed));
        completedHandler->Release();
        auto* messageHandler = new Callback<ICoreWebView2WebMessageReceivedEventHandler, ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs*>(
            [self](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* event) {
                if (!self->usable() || self->failed || !event) return S_OK;
                LPWSTR uri = nullptr, json = nullptr;
                if (SUCCEEDED(event->get_Source(&uri)) && localDocument(uri) && SUCCEEDED(event->get_WebMessageAsJson(&json))) {
                    auto text = narrow(json);
                    if (!text.empty()) self->incoming(std::move(text));
                }
                CoTaskMemFree(uri); CoTaskMemFree(json);
                return S_OK;
            });
        hasMessage = SUCCEEDED(view->add_WebMessageReceived(messageHandler, &message));
        messageHandler->Release();
        auto* newWindowHandler = new Callback<ICoreWebView2NewWindowRequestedEventHandler, ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs*>(
            [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* event) { if (event) event->put_Handled(TRUE); return S_OK; });
        hasNewWindow = SUCCEEDED(view->add_NewWindowRequested(newWindowHandler, &newWindow));
        newWindowHandler->Release();
        auto* permissionHandler = new Callback<ICoreWebView2PermissionRequestedEventHandler, ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs*>(
            [](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* event) { if (event) event->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY); return S_OK; });
        hasPermission = SUCCEEDED(view->add_PermissionRequested(permissionHandler, &permission));
        permissionHandler->Release();
        auto* processHandler = new Callback<ICoreWebView2ProcessFailedEventHandler, ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs*>(
            [self](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs*) { self->error(L"控制台界面进程已停止，请重新打开工具箱。"); return S_OK; });
        hasProcess = SUCCEEDED(view->add_ProcessFailed(processHandler, &process));
        processHandler->Release();
        if (SUCCEEDED(view->QueryInterface(interfaceId<ICoreWebView2_4>(), reinterpret_cast<void**>(&downloads))) && downloads) {
            auto* downloadHandler = new Callback<ICoreWebView2DownloadStartingEventHandler, ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs*>(
                [](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* event) { if (event) { event->put_Cancel(TRUE); event->put_Handled(TRUE); } return S_OK; });
            hasDownload = SUCCEEDED(downloads->add_DownloadStarting(downloadHandler, &download));
            downloadHandler->Release();
        }
        if (!hasNavigation || !hasFrameNavigation || !hasCompleted || !hasMessage || !hasNewWindow || !hasPermission || !hasProcess || !hasDownload) {
            error(L"WebView2 运行环境版本过旧或初始化失败，请更新后重试。"); return S_OK;
        }
        const auto html = widen(consoleassets::Html, consoleassets::HtmlSize);
        documentRequestUri=embeddedRequestUri(consoleassets::Html,consoleassets::HtmlSize);documentNavigationId=0;
        if (html.empty() || documentRequestUri.empty() || FAILED(controller->put_Bounds(bounds)) || FAILED(controller->put_IsVisible(TRUE)))
            error(L"控制台页面初始化失败，请重新打开工具箱。");
        else {documentPending=true;const HRESULT navigated=view->NavigateToString(html.c_str());if(FAILED(navigated)){wchar_t detail[160]{};std::swprintf(detail,160,L"控制台页面初始化失败：NavigateToString HRESULT=0x%08lX。",static_cast<unsigned long>(navigated));error(detail);}}
        return S_OK;
    }
};

Host::Host() = default;
Host::~Host() { close(); }

bool Host::create(HWND parent, MessageHandler onMessage, ErrorHandler onError) {
    if (state_ && GetCurrentThreadId() != state_->owner) return false;
    close();
    if (!parent || !IsWindow(parent) || GetWindowThreadProcessId(parent, nullptr) != GetCurrentThreadId()) return false;
    auto state = std::make_shared<State>();
    state->parent = parent; state->onMessage = std::move(onMessage); state->onError = std::move(onError);
    state_ = state;
    GetClientRect(parent, &state->bounds);
    if (!state->createDispatcher()) {
        if (state->onError) state->onError(L"无法创建控制台消息通道，请重新打开工具箱。");
        close(); return false;
    }
    const auto directory = moduleDirectory();
    if (directory.empty()) { state->error(L"无法确定工具箱目录，请完整解压后重试。"); return false; }
    const auto path = directory + L"\\WebView2Loader.dll";
    state->loader = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!state->loader) { state->error(L"缺少 WebView2Loader.dll，请完整解压工具箱后重试。"); return false; }
    using CreateEnvironment = HRESULT (STDAPICALLTYPE*)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
    const auto createEnvironment = reinterpret_cast<CreateEnvironment>(GetProcAddress(state->loader, "CreateCoreWebView2EnvironmentWithOptions"));
    if (!createEnvironment) { state->error(L"WebView2Loader.dll 不兼容，请重新解压工具箱。"); return false; }
    using AvailableVersion = HRESULT (STDAPICALLTYPE*)(PCWSTR, LPWSTR*);
    const auto availableVersion = reinterpret_cast<AvailableVersion>(GetProcAddress(state->loader, "GetAvailableCoreWebView2BrowserVersionString"));
    LPWSTR version = nullptr;
    const bool available = availableVersion && SUCCEEDED(availableVersion(nullptr, &version)) && version && version[0];
    CoTaskMemFree(version);
    if (!available) { state->error(L"未检测到 Microsoft Edge WebView2 运行环境。请安装或更新 WebView2 Evergreen Runtime 后重新打开工具箱。"); return false; }
    state->dataDirectory = userDataDirectory();
    if (state->dataDirectory.empty()) { state->error(L"无法创建控制台缓存目录，请检查当前用户的本地应用数据目录权限。"); return false; }
    auto* handler = new Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, HRESULT, ICoreWebView2Environment*>(
        [state](HRESULT result, ICoreWebView2Environment* environment) {
            if (!state->usable()) return S_OK;
            if (FAILED(result) || !environment) { state->error(L"WebView2 无法启动，请更新运行环境后重新打开工具箱。"); return S_OK; }
            state->environment = environment; environment->AddRef();
            auto* controllerHandler = new Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, HRESULT, ICoreWebView2Controller*>(
                [state](HRESULT result, ICoreWebView2Controller* controller) { return state->initializeController(result, controller); });
            const HRESULT started = environment->CreateCoreWebView2Controller(state->parent, controllerHandler);
            controllerHandler->Release();
            if (FAILED(started)) state->error(L"无法启动控制台界面，请重新打开工具箱。");
            return S_OK;
        });
    const HRESULT started = createEnvironment(nullptr, state->dataDirectory.c_str(), nullptr, handler);
    handler->Release();
    if (FAILED(started)) { state->error(L"WebView2 初始化失败，请确认已安装 Microsoft Edge WebView2 运行环境。"); return false; }
    return true;
}

void Host::resize(const RECT& bounds) {
    if (!state_ || !state_->usable()) return;
    state_->bounds = bounds;
    if (state_->controller) state_->controller->put_Bounds(bounds);
}

bool Host::post(const std::string& json) {
    if (!state_ || !state_->usable() || !state_->pageReady || !state_->view || json.empty() || json.size() > OutgoingLimit) return false;
    const auto text = widen(json.data(), json.size());
    return !text.empty() && SUCCEEDED(state_->view->PostWebMessageAsJson(text.c_str()));
}

bool Host::ready() const { return state_ && state_->usable() && !state_->failed && state_->pageReady; }

void Host::close() {
    if (!state_ || GetCurrentThreadId() != state_->owner) return;
    auto state = std::move(state_);
    state->dispose();
}
}
