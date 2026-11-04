#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <winioctl.h>
#include <atomic>
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cwctype>
#include <cstring>
#include <iterator>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#define IDI_APP 1
#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

struct SYMBOLIC11_REPARSE_BUFFER {
    ULONG ReparseTag; USHORT ReparseDataLength; USHORT Reserved;
    union {
        struct { USHORT SubstituteNameOffset, SubstituteNameLength, PrintNameOffset, PrintNameLength; ULONG Flags; WCHAR PathBuffer[1]; } SymbolicLinkReparseBuffer;
        struct { USHORT SubstituteNameOffset, SubstituteNameLength, PrintNameOffset, PrintNameLength; WCHAR PathBuffer[1]; } MountPointReparseBuffer;
    };
};

namespace fs = std::filesystem;
static constexpr UINT WM_SCAN_ROW = WM_APP + 10;
static constexpr UINT WM_SCAN_DONE = WM_APP + 11;
static constexpr UINT WM_SCAN_PROGRESS = WM_APP + 12;
static constexpr COLORREF BG = RGB(32,32,32), PANEL = RGB(43,43,43), FIELD = RGB(62,62,62), FG = RGB(242,242,242), MUTED = RGB(185,185,185), ACCENT = RGB(0,153,220), BORDER = RGB(75,75,75);

struct LinkRow { std::wstring source, type, target; bool valid=false; };
struct App {
    HWND hwnd{}, tabs{}, createPage{}, viewPage{}, navCreate{}, navView{};
    HWND type{}, fileType{}, linkFolder{}, destPath{}, linkName{}, createBtn{}, info{}, guideBtn{}, useNameBtn{}, linkFolderLabel{}, destLabel{};
    HWND searchPath{}, searchBtn{}, grid{}, task{}, deleteBtn{};
    HFONT font{}, titleFont{};
    HBRUSH bgBrush{}, panelBrush{}, fieldBrush{};
    std::atomic_bool scanning{false}, cancelScan{false};
    std::thread worker;
    std::vector<LinkRow> rows;
    std::wstring searchRoot=L"C:\\Users\\";
    bool guide=false;
} g;

static std::wstring lower(std::wstring s){ std::transform(s.begin(),s.end(),s.begin(),[](wchar_t c){return (wchar_t)towlower(c);}); return s; }
static std::wstring basename(const std::wstring& p){ return fs::path(p).filename().wstring(); }
static std::wstring extension(const std::wstring& p){ return fs::path(p).extension().wstring(); }
static void setFont(HWND h){ if(h && g.font) SendMessageW(h,WM_SETFONT,(WPARAM)g.font,TRUE); }
static HWND ctl(HWND parent,const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id){ HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,parent,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr); setFont(c); if(c)SetWindowTheme(c,L"DarkMode_Explorer",nullptr); return c; }
static void setText(HWND h,const std::wstring& s){ SetWindowTextW(h,s.c_str()); }
static std::wstring getText(HWND h){ int n=GetWindowTextLengthW(h); std::wstring s((size_t)n+1,L'\0'); if(n) GetWindowTextW(h,s.data(),n+1); s.resize((size_t)n); return s; }
static void status(const std::wstring& title,const std::wstring& message=L""){ std::wstring s=title; if(!message.empty()) s+=L"  "+message; setText(g.info,s); InvalidateRect(g.info,nullptr,TRUE); }
static bool isAdmin(){ BOOL yes=FALSE; PSID group=nullptr; SID_IDENTIFIER_AUTHORITY nt=SECURITY_NT_AUTHORITY; if(AllocateAndInitializeSid(&nt,2,SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0,0,0,0,0,0,&group)){ CheckTokenMembership(nullptr,group,&yes); FreeSid(group);} return yes!=FALSE; }
static bool developerMode(){ HKEY k{}; DWORD v=0,n=sizeof(v); bool ok=RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock",0,KEY_READ,&k)==ERROR_SUCCESS; if(ok){ok=RegQueryValueExW(k,L"AllowDevelopmentWithoutDevLicense",nullptr,nullptr,(BYTE*)&v,&n)==ERROR_SUCCESS && v==1; RegCloseKey(k);} return ok; }
static std::wstring winError(DWORD e){ wchar_t* b=nullptr; FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,e,0,(LPWSTR)&b,0,nullptr); std::wstring s=b?b:L"Windows error "+std::to_wstring(e); if(b) LocalFree(b); while(!s.empty()&&(s.back()==L'\r'||s.back()==L'\n'||s.back()==L' '))s.pop_back(); return s; }
static bool browseItem(HWND owner, bool folder, std::wstring& out, const wchar_t* title = nullptr){
    IFileOpenDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr)) return false;

    DWORD options = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
    if (folder) options |= FOS_PICKFOLDERS;
    else options |= FOS_FILEMUSTEXIST;
    dlg->SetOptions(options);

    if (title) dlg->SetTitle(title);

    if (!out.empty()) {
        IShellItem* initialItem = nullptr;
        std::error_code ec;
        std::wstring initialPath = out;
        if (!folder && fs::is_regular_file(fs::path(initialPath), ec)) {
            initialPath = fs::path(initialPath).parent_path().wstring();
        }
        if (SUCCEEDED(SHCreateItemFromParsingName(initialPath.c_str(), nullptr, IID_PPV_ARGS(&initialItem)))) {
            dlg->SetFolder(initialItem);
            initialItem->Release();
        }
    }

    hr = dlg->Show(owner);
    if (SUCCEEDED(hr)) {
        IShellItem* result = nullptr;
        if (SUCCEEDED(dlg->GetResult(&result))) {
            PWSTR pszPath = nullptr;
            if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                out = pszPath;
                CoTaskMemFree(pszPath);
            }
            result->Release();
        }
    }
    dlg->Release();
    return SUCCEEDED(hr);
}
static bool browseFolder(HWND owner,std::wstring& out,const wchar_t* title=nullptr){ return browseItem(owner, true, out, title); }
static bool browseFile(HWND owner,std::wstring& out,const wchar_t* title=nullptr){ return browseItem(owner, false, out, title); }
static bool createHardLink(const std::wstring& link,const std::wstring& target){ return CreateHardLinkW(link.c_str(),target.c_str(),nullptr)!=FALSE; }
static bool createSymbolic(const std::wstring& link,const std::wstring& target,bool dir){ DWORD flags=dir?SYMBOLIC_LINK_FLAG_DIRECTORY:0;
#ifdef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
 flags|=SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
#endif
 BOOL ok=CreateSymbolicLinkW(link.c_str(),target.c_str(),flags); if(!ok && (flags & SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) && GetLastError()==ERROR_INVALID_PARAMETER) ok=CreateSymbolicLinkW(link.c_str(),target.c_str(),dir?SYMBOLIC_LINK_FLAG_DIRECTORY:0); return ok!=FALSE; }
static bool createJunction(const std::wstring& link,const std::wstring& target,DWORD& err){ if(!CreateDirectoryW(link.c_str(),nullptr)){err=GetLastError();return false;} HANDLE h=CreateFileW(link.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,nullptr); if(h==INVALID_HANDLE_VALUE){err=GetLastError();RemoveDirectoryW(link.c_str());return false;}
 std::wstring abs=fs::absolute(target).wstring(); std::wstring substitute=L"\\??\\"+abs; std::wstring print=abs; size_t subBytes=substitute.size()*sizeof(wchar_t), printBytes=print.size()*sizeof(wchar_t); size_t dataLen=8+subBytes+sizeof(wchar_t)+printBytes+sizeof(wchar_t); std::vector<BYTE> buf(8+dataLen); auto* r=(SYMBOLIC11_REPARSE_BUFFER*)buf.data(); r->ReparseTag=IO_REPARSE_TAG_MOUNT_POINT; r->Reserved=0; r->MountPointReparseBuffer.SubstituteNameOffset=0; r->MountPointReparseBuffer.SubstituteNameLength=(USHORT)subBytes; r->MountPointReparseBuffer.PrintNameOffset=(USHORT)(subBytes+sizeof(wchar_t)); r->MountPointReparseBuffer.PrintNameLength=(USHORT)printBytes; memcpy(r->MountPointReparseBuffer.PathBuffer,substitute.c_str(),subBytes+sizeof(wchar_t)); memcpy((BYTE*)r->MountPointReparseBuffer.PathBuffer+subBytes+sizeof(wchar_t),print.c_str(),printBytes+sizeof(wchar_t)); r->ReparseDataLength=(USHORT)(dataLen-8); DWORD returned=0; BOOL ok=DeviceIoControl(h,FSCTL_SET_REPARSE_POINT,r,(DWORD)buf.size(),nullptr,0,&returned,nullptr); err=ok?0:GetLastError(); CloseHandle(h); if(!ok)RemoveDirectoryW(link.c_str()); return ok; }
static bool readReparseTarget(const std::wstring& path,std::wstring& target,std::wstring& type){
 HANDLE h=CreateFileW(path.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,nullptr); if(h==INVALID_HANDLE_VALUE)return false;
 std::vector<BYTE> b(MAXIMUM_REPARSE_DATA_BUFFER_SIZE); DWORD n=0; bool ok=DeviceIoControl(h,FSCTL_GET_REPARSE_POINT,nullptr,0,b.data(),(DWORD)b.size(),&n,nullptr)!=FALSE; CloseHandle(h); if(!ok||n<8)return false;
 auto* r=(SYMBOLIC11_REPARSE_BUFFER*)b.data();
 if(r->ReparseTag==IO_REPARSE_TAG_SYMLINK){ if(n<20)return false; type=L"SymLink"; auto& q=r->SymbolicLinkReparseBuffer; DWORD pathBytes=n-20; if((q.SubstituteNameOffset|q.SubstituteNameLength)&1 || (DWORD)q.SubstituteNameOffset+q.SubstituteNameLength>pathBytes)return false; target.assign(q.PathBuffer+q.SubstituteNameOffset/2,q.SubstituteNameLength/2); if(target.rfind(L"\\??\\UNC\\",0)==0)target=L"\\\\"+target.substr(8); else if(target.rfind(L"\\??\\",0)==0)target.erase(0,4); if(q.Flags&SYMLINK_FLAG_RELATIVE)target=(fs::path(path).parent_path()/target).lexically_normal().wstring(); }
 else if(r->ReparseTag==IO_REPARSE_TAG_MOUNT_POINT){ if(n<16)return false; type=L"Junction"; auto& q=r->MountPointReparseBuffer; DWORD pathBytes=n-16; if((q.SubstituteNameOffset|q.SubstituteNameLength)&1 || (DWORD)q.SubstituteNameOffset+q.SubstituteNameLength>pathBytes)return false; target.assign(q.PathBuffer+q.SubstituteNameOffset/2,q.SubstituteNameLength/2); if(target.rfind(L"\\??\\UNC\\",0)==0)target=L"\\\\"+target.substr(8); else if(target.rfind(L"\\??\\",0)==0)target.erase(0,4); }
 else { type=L"Reparse"; target=L"(unsupported reparse point)"; }
 return true;
}
static std::wstring shortcutTarget(const std::wstring& path){ IShellLinkW* link=nullptr; std::wstring result; if(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link)))){ IPersistFile* pf=nullptr; if(SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&pf)))){ if(SUCCEEDED(pf->Load(path.c_str(),STGM_READ))){ wchar_t p[32768]{}; WIN32_FIND_DATAW fd{}; if(SUCCEEDED(link->GetPath(p,(int)std::size(p),&fd,SLGP_RAWPATH)))result=p; } pf->Release(); } link->Release(); } return result; }
static void scanDirectory(fs::path root,int depth){ if(g.cancelScan.load()||depth>5)return; std::error_code ec; fs::directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end; for(;it!=end&&!g.cancelScan.load();it.increment(ec)){ if(ec){ec.clear();continue;} const auto p=it->path(); DWORD a=GetFileAttributesW(p.c_str()); if(a==INVALID_FILE_ATTRIBUTES)continue; if(a&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_READONLY))continue; PostMessageW(g.hwnd,WM_SCAN_PROGRESS,0,(LPARAM)new std::wstring(p.wstring()));
   LinkRow row; row.source=p.wstring(); bool found=false; if(a&FILE_ATTRIBUTE_REPARSE_POINT){ found=readReparseTarget(row.source,row.target,row.type); if(found){ std::error_code e; row.valid=fs::exists(fs::path(row.target),e); } } else if(lower(p.extension().wstring())==L".lnk"){ row.type=L"Shortcut"; row.target=shortcutTarget(row.source); found=!row.target.empty(); std::error_code e; row.valid=found&&fs::exists(fs::path(row.target),e); }
   if(found)PostMessageW(g.hwnd,WM_SCAN_ROW,0,(LPARAM)new LinkRow(std::move(row)));
   if((a&FILE_ATTRIBUTE_DIRECTORY)&&!(a&FILE_ATTRIBUTE_REPARSE_POINT)&&depth<5)scanDirectory(p,depth+1);
 }
}
static void startScan(){ if(g.scanning.exchange(true)){g.cancelScan=true;setText(g.searchBtn,L"Search");setText(g.task,L"");return;} if(g.worker.joinable())g.worker.join(); g.cancelScan=false; g.rows.clear(); ListView_DeleteAllItems(g.grid); setText(g.searchBtn,L"Cancel"); g.searchRoot=getText(g.searchPath); setText(g.task,L""); g.worker=std::thread([]{ CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); scanDirectory(fs::path(g.searchRoot),0); CoUninitialize(); PostMessageW(g.hwnd,WM_SCAN_DONE,0,0); }); }
static void addRow(const LinkRow& r){ int i=ListView_GetItemCount(g.grid); LVITEMW item{}; item.mask=LVIF_TEXT; item.iItem=i; item.pszText=(LPWSTR)r.source.c_str(); ListView_InsertItem(g.grid,&item); ListView_SetItemText(g.grid,i,1,(LPWSTR)r.type.c_str()); ListView_SetItemText(g.grid,i,2,(LPWSTR)r.target.c_str()); std::wstring v=r.valid?L"Yes":L"No"; ListView_SetItemText(g.grid,i,3,(LPWSTR)v.c_str()); g.rows.push_back(r); }
static LRESULT CALLBACK pageProc(HWND h,UINT m,WPARAM wp,LPARAM lp);
static void createControls(HWND w){
 ctl(w,L"STATIC",L"Folder Junction",SS_LEFT,14,22,120,28,40);
 g.navCreate=ctl(w,L"BUTTON",L"Create",BS_PUSHBUTTON,10,76,125,38,2);
 g.navView=ctl(w,L"BUTTON",L"View",BS_PUSHBUTTON,10,120,125,38,3);
 g.createPage=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE,145,0,495,381,w,nullptr,nullptr,nullptr); g.viewPage=CreateWindowExW(0,L"STATIC",L"",WS_CHILD,145,0,495,381,w,nullptr,nullptr,nullptr); SetWindowLongPtrW(g.createPage,GWLP_WNDPROC,(LONG_PTR)pageProc); SetWindowLongPtrW(g.viewPage,GWLP_WNDPROC,(LONG_PTR)pageProc);
 // Create page: two selector groups and guide toggle.
 ctl(g.createPage,L"STATIC",L"Create a",0,12,2,100,22,10); g.type=ctl(g.createPage,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,12,25,145,180,11); SendMessageW(g.type,CB_ADDSTRING,0,(LPARAM)L"Symbolic Link");SendMessageW(g.type,CB_ADDSTRING,0,(LPARAM)L"Hard Link");SendMessageW(g.type,CB_ADDSTRING,0,(LPARAM)L"Junction Link");SendMessageW(g.type,CB_SETCURSEL,0,0);
 ctl(g.createPage,L"STATIC",L"for a",0,170,2,80,22,12); g.fileType=ctl(g.createPage,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,170,25,130,180,13);SendMessageW(g.fileType,CB_ADDSTRING,0,(LPARAM)L"Folder");SendMessageW(g.fileType,CB_ADDSTRING,0,(LPARAM)L"File");SendMessageW(g.fileType,CB_SETCURSEL,0,0);
 g.guideBtn=ctl(g.createPage,L"BUTTON",L"Show Guide  ?",BS_PUSHBUTTON,340,10,135,34,14);
 g.linkFolderLabel=ctl(g.createPage,L"STATIC",L"Link Folder",0,12,66,180,20,15); g.linkFolder=ctl(g.createPage,L"EDIT",L"",ES_AUTOHSCROLL|ES_READONLY|WS_TABSTOP,12,88,345,34,16); ctl(g.createPage,L"BUTTON",L"Explore",BS_PUSHBUTTON,370,88,105,34,17);
 g.destLabel=ctl(g.createPage,L"STATIC",L"Destination Folder",0,12,168,200,20,18); g.destPath=ctl(g.createPage,L"EDIT",L"",ES_AUTOHSCROLL|ES_READONLY|WS_TABSTOP,12,190,345,34,19); ctl(g.createPage,L"BUTTON",L"Explore",BS_PUSHBUTTON,370,190,105,34,20);
 g.linkName=ctl(g.createPage,L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,12,245,245,34,21); SendMessageW(g.linkName,EM_SETCUEBANNER,TRUE,(LPARAM)L"Enter link name"); g.useNameBtn=ctl(g.createPage,L"BUTTON",L"Use Destination Name",BS_PUSHBUTTON,265,245,190,34,22);
 g.info=ctl(g.createPage,L"STATIC",L"",SS_LEFT,12,300,335,30,23); g.createBtn=ctl(g.createPage,L"BUTTON",L"Create Link",BS_PUSHBUTTON|WS_TABSTOP,370,295,105,38,24); EnableWindow(g.createBtn,FALSE);
 // View page.
 ctl(g.viewPage,L"STATIC",L"Search Directory:",0,12,0,180,22,30); g.searchPath=ctl(g.viewPage,L"EDIT",L"C:\\Users\\",ES_READONLY|ES_AUTOHSCROLL,12,24,260,32,31); ctl(g.viewPage,L"BUTTON",L"Browse",BS_PUSHBUTTON,280,24,85,32,32); g.searchBtn=ctl(g.viewPage,L"BUTTON",L"Search",BS_PUSHBUTTON,373,24,102,32,33);
 g.grid=CreateWindowExW(WS_EX_CLIENTEDGE,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,12,68,463,218,g.viewPage,(HMENU)34,GetModuleHandleW(nullptr),nullptr); SetWindowTheme(g.grid,L"DarkMode_Explorer",nullptr); ListView_SetBkColor(g.grid,PANEL); ListView_SetTextBkColor(g.grid,PANEL); ListView_SetTextColor(g.grid,FG); ListView_SetExtendedListViewStyle(g.grid,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_GRIDLINES); const wchar_t* heads[]={L"Source",L"Type",L"Target",L"Result"}; int widths[]={155,65,185,55}; for(int i=0;i<4;i++){LVCOLUMNW c{};c.mask=LVCF_TEXT|LVCF_WIDTH;c.pszText=(LPWSTR)heads[i];c.cx=widths[i];ListView_InsertColumn(g.grid,i,&c);} ctl(g.viewPage,L"STATIC",L"Current Task:",0,12,292,150,20,35); g.task=ctl(g.viewPage,L"EDIT",L"",ES_READONLY|ES_AUTOHSCROLL,12,312,345,28,36); g.deleteBtn=ctl(g.viewPage,L"BUTTON",L"Delete",BS_PUSHBUTTON,370,310,105,32,37); EnableWindow(g.deleteBtn,FALSE);
}
static void switchPage(int n){ ShowWindow(g.createPage,n==0?SW_SHOW:SW_HIDE);ShowWindow(g.viewPage,n==1?SW_SHOW:SW_HIDE); EnableWindow(g.navCreate,n!=0); EnableWindow(g.navView,n!=1); }
static void updateType(){ int ft=(int)SendMessageW(g.fileType,CB_GETCURSEL,0,0); int t=(int)SendMessageW(g.type,CB_GETCURSEL,0,0); if((ft==0&&t==1)||(ft==1&&t==2))SendMessageW(g.type,CB_SETCURSEL,0,0); }
static LRESULT CALLBACK pageProc(HWND h,UINT m,WPARAM wp,LPARAM lp){ if(m==WM_COMMAND){int id=LOWORD(wp);
 if(id==32){std::wstring p=getText(g.searchPath);if(browseFolder(g.hwnd,p,L"Select Search Directory"))setText(g.searchPath,p);return 0;}
 if(id==33){startScan();return 0;}
 if(id==37){int i=ListView_GetNextItem(g.grid,-1,LVNI_SELECTED);if(i>=0&&i<(int)g.rows.size()){auto r=g.rows[i];int answer=MessageBoxW(g.hwnd,(L"Delete link entry?\n\n"+r.source+L"\n\nThe target will not be deleted.").c_str(),L"Confirm deletion",MB_ICONWARNING|MB_YESNO|MB_DEFBUTTON2);if(answer==IDYES){DWORD a=GetFileAttributesW(r.source.c_str());BOOL ok=(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY))?RemoveDirectoryW(r.source.c_str()):DeleteFileW(r.source.c_str());if(ok){ListView_DeleteItem(g.grid,i);g.rows.erase(g.rows.begin()+i);status(L"Link deleted.");}else status(L"Delete failed",winError(GetLastError()));}}return 0;}
 if(id==17||id==20){bool folder=id==17?true:(SendMessageW(g.fileType,CB_GETCURSEL,0,0)==0); std::wstring p=getText(id==17?g.linkFolder:g.destPath); const wchar_t* title=id==17?L"Select Link Location Folder":(folder?L"Select Destination Folder":L"Select Destination File"); if(folder?browseFolder(g.hwnd,p,title):browseFile(g.hwnd,p,title)){setText(id==17?g.linkFolder:g.destPath,p); EnableWindow(g.createBtn,!getText(g.linkFolder).empty()&&!getText(g.destPath).empty());} return 0;}
 if(id==22){auto p=getText(g.destPath); if(!p.empty()){auto n=basename(p); if(lower(extension(p))!=L"")n=fs::path(p).stem().wstring();setText(g.linkName,n);}return 0;}
 if(id==14){g.guide=!g.guide;setText(g.guideBtn,g.guide?L"Hide Guide  ?":L"Show Guide  ?"); status(g.guide?L"The link is created at the selected Link Folder.":L"",g.guide?L"It points to the selected Destination.":L""); return 0;}
 if(id==24){auto name=getText(g.linkName),folder=getText(g.linkFolder),dest=getText(g.destPath); if(name.empty()||name.find_first_of(L"\\/:*?\"<>|")!=std::wstring::npos){status(L"Invalid Name!",L"Enter a valid link name.");return 0;} if(folder.empty()||dest.empty()){status(L"Missing path",L"Select both folders/paths.");return 0;} int t=(int)SendMessageW(g.type,CB_GETCURSEL,0,0), ft=(int)SendMessageW(g.fileType,CB_GETCURSEL,0,0); bool dir=ft==0; if((t==1&&dir)||(t==2&&!dir)){status(L"Invalid link type",L"Selected type is not valid for this file type.");return 0;} fs::path target(dest); std::wstring link=(fs::path(folder)/name).wstring(); if(t==1)link+=target.extension().wstring(); DWORD err=0; bool ok=false; DWORD targetAttr=GetFileAttributesW(dest.c_str()); if(targetAttr==INVALID_FILE_ATTRIBUTES){status(L"Target not found",winError(GetLastError()));return 0;} if(dir && !(targetAttr&FILE_ATTRIBUTE_DIRECTORY)){status(L"Invalid destination",L"A folder is required for this link type.");return 0;} if(!dir && (targetAttr&FILE_ATTRIBUTE_DIRECTORY)){status(L"Invalid destination",L"A file is required for this link type.");return 0;} if(GetFileAttributesW(link.c_str())!=INVALID_FILE_ATTRIBUTES){status(L"Link already exists",link);return 0;} if(t==0)ok=createSymbolic(link,dest,dir); else if(t==1)ok=createHardLink(link,dest); else ok=createJunction(link,dest,err); if(ok)status(L"Link "+name+L" Created!"); else {if(!err)err=GetLastError();status(L"Error Creating Link",winError(err));} return 0; }
 if(id==13){updateType();setText(g.linkFolderLabel,SendMessageW(g.type,CB_GETCURSEL,0,0)==1?L"Link File":L"Link Folder");return 0;}
 if(id==11){int ft=(int)SendMessageW(g.fileType,CB_GETCURSEL,0,0); updateType(); setText(g.destLabel,ft==0?L"Destination Folder":L"Destination File"); return 0;}
 }
 if(m==WM_ERASEBKGND){RECT r;GetClientRect(h,&r);FillRect((HDC)wp,&r,g.bgBrush);return 1;}
 if(m==WM_CTLCOLORSTATIC||m==WM_CTLCOLOREDIT||m==WM_CTLCOLORLISTBOX){HDC dc=(HDC)wp;SetTextColor(dc,FG);SetBkColor(dc,m==WM_CTLCOLORSTATIC?BG:FIELD);return (LRESULT)(m==WM_CTLCOLORSTATIC?g.bgBrush:g.fieldBrush);}
 if(m==WM_NOTIFY && ((NMHDR*)lp)->idFrom==34){auto* n=(NMHDR*)lp;if(n->code==LVN_ITEMCHANGED){int sel=ListView_GetNextItem(g.grid,-1,LVNI_SELECTED);EnableWindow(g.deleteBtn,sel>=0);} if(n->code==NM_DBLCLK){int sel=ListView_GetNextItem(g.grid,-1,LVNI_SELECTED);if(sel>=0){wchar_t p[32768]{};ListView_GetItemText(g.grid,sel,0,p,(int)std::size(p));std::wstring arg=L"/select,\""+std::wstring(p)+L"\"";ShellExecuteW(g.hwnd,L"open",L"explorer.exe",arg.c_str(),nullptr,SW_SHOWNORMAL);}} }
 return DefWindowProcW(h,m,wp,lp); }
static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM wp,LPARAM lp){ switch(m){case WM_CREATE:{g.hwnd=h; g.bgBrush=CreateSolidBrush(BG);g.panelBrush=CreateSolidBrush(PANEL);g.fieldBrush=CreateSolidBrush(FIELD); g.font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,VARIABLE_PITCH,L"Segoe UI Variable");g.titleFont=CreateFontW(-19,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,VARIABLE_PITCH,L"Segoe UI Variable");createControls(h); switchPage(0); if(!developerMode()&&!isAdmin())status(L"Insufficient Permissions",L"Link creation may require administrator privileges."); return 0;}
 case WM_COMMAND:{int id=LOWORD(wp);if(id==2){switchPage(0);return 0;}if(id==3){switchPage(1);return 0;}break;}
 case WM_SCAN_ROW:{std::unique_ptr<LinkRow> r((LinkRow*)lp);addRow(*r);return 0;}case WM_SCAN_PROGRESS:{std::unique_ptr<std::wstring> p((std::wstring*)lp);setText(g.task,*p);return 0;}case WM_SCAN_DONE:{g.scanning=false;setText(g.searchBtn,L"Search");setText(g.task,L"");if(g.cancelScan)status(L"Search cancelled",std::to_wstring(g.rows.size())+L" links found.");else status(L"Search completed",std::to_wstring(g.rows.size())+L" links found.");return 0;}
 case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{HDC dc=(HDC)wp;SetTextColor(dc,FG);SetBkColor(dc,(m==WM_CTLCOLORSTATIC)?PANEL:FIELD);return (LRESULT)((m==WM_CTLCOLORSTATIC)?g.panelBrush:g.fieldBrush);}
 case WM_ERASEBKGND:{RECT r;GetClientRect(h,&r);FillRect((HDC)wp,&r,g.bgBrush);return 1;}
 case WM_DESTROY:{g.cancelScan=true;if(g.worker.joinable())g.worker.join();DeleteObject(g.font);DeleteObject(g.titleFont);DeleteObject(g.bgBrush);DeleteObject(g.panelBrush);DeleteObject(g.fieldBrush);PostQuitMessage(0);return 0;}}
 return DefWindowProcW(h,m,wp,lp); }
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE, PWSTR,int show){ CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_WIN95_CLASSES|ICC_TAB_CLASSES|ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&ic); WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=WndProc;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(IDI_APP));wc.hIconSm=wc.hIcon;wc.hbrBackground=nullptr;wc.lpszClassName=L"FolderJunctionNativeWindow";RegisterClassExW(&wc); HWND h=CreateWindowExW(0,wc.lpszClassName,L"Folder Junction",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,650,420,nullptr,nullptr,instance,nullptr); if(!h)return 1; BOOL dark=TRUE; if(FAILED(DwmSetWindowAttribute(h,20,&dark,sizeof(dark))))DwmSetWindowAttribute(h,19,&dark,sizeof(dark)); ShowWindow(h,show);UpdateWindow(h); MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);} CoUninitialize();return (int)msg.wParam; }
