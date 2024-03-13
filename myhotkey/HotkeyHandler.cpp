/*
  History
  ----------
  Monday, April 21, 2003
             * Initial version

  Tuesday, April 22, 2003
             * Added list entries recycling into RemoveHandler(). Now removing works correctly.
             * Added utility functions: HotkeyFlagsToModifiers() and HotkeyModifiersToFlags()
  Wednesday, April 23, 2003
             * Added checks on Start() to prevent starting if already started.
  Thursday, April 24, 2003
             * Fixed CreateThread() in Start() calling so that it works on Win9x.
  Thursday, May 01, 2003
             * Fixed code in MakeWindow() and now CHotkeyHandler works under Win9x very well.
  Monday, Oct 20, 2003
             * Removed 'using std::vector' from header file
             * Fixed a minor bug in RemoveHandler()
*/

/* -----------------------------------------------------------------------------
 * Copyright (c) 2003 Lallous <lallousx86@yahoo.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -----------------------------------------------------------------------------
 */

/*
  CHotkeyHandler() is a class that wrappes the RegisterHotkey() and its
  UnregisterHotkey() Windows APIs.
  This class provides a simple method to implement and handle multiple hotkeys at the same time.


  * Debug Flag:
  --------------
  bool bDebug;
  This flag is used to turn debugging on/off. Useful when you're developing.

  * Constructor
  ---------------
  CHotkeyHandler(bool Debug = false);
  This constructor initializes internal variables and sets debug info on/off
  If you call Start() and do not call Stop() the destructor will automatically Stop() for you.

  * Start()
  -----------
  int Start(LPVOID Param=NULL);
  This method is asynchronous; It starts the hotkey listener.
  You must have at least one handler inserted before calling this method, otherwise hkheNoEntry
  would be returned.
  It returns an hkhe error code.
  'Param' is a user supplied pointer used when calling the hotkey callbacks.

  * Stop()
  -----------
  int Stop();
  After you've started the hotkey listener, you can stop it w/ Stop().
  Stop() will return success if it was stopped successfully or if it was not started at all.


  * InsertHandler()
  --------------------
  int InsertHandler(WORD mod, WORD virt, tHotkeyCB cb, int &index);
  This function will return an error code.
  'index' will be filled by the index of where the hotkey is stored (1 based).
  If you attempt to insert a hotkey that already exists then 'index' will be the index value
  of that previously inserted hotkey.

  'mod' is any of the MOD_XXXX constants.
  'virt' is the virtual key code.
  'cb' is the handler that will be called. The prototype is: void Callback(void *param)
  The 'param' of 'cb' will be the one that you passed when you called Start()

  * RemoveHandler()
  --------------------
  int RemoveHandler(const int index);
  Removes a hotkey definition. This function has effects only when called before Start()
  or after Stop().

  * HotkeyModifiersToFlags()
  ----------------------------
  WORD HotkeyModifiersToFlags(WORD modf);
  Converts from MOD_XXX modifiers into HOTKEYF_XXXX to be used by HOTKEYFLAGS.
  This method is static.

  * HotkeyFlagsToModifiers()
  -----------------------------
  WORD HotkeyFlagsToModifiers(WORD hkf);
  Converts HOTKEYFLAGS used by CHotKeyCtrl into MOD_XXX used by windows API
  This method is static.



  Error codes
  -------------
  CHotkeyHandler::hkheXXXXX
    hkheOk             - Success
    hkheClassError     - Window class registration error
    hkheWindowError    - Window creation error
    hkheNoEntry        - No entry found at given index
    hkheRegHotkeyError - Could not register hotkey
    hkheMessageLoop    - Could not create message loop thread
    hkheInternal       - Internal error

*/


#include "hotkeyhandler.h"
#include <string>
using namespace std;

//-------------------------------------------------------------------------------------
// Initializes internal variables
CHotkeyHandler::CHotkeyHandler(bool Debug)
{
  bDebug     = Debug;
  m_bStarted = false;
  m_hWnd     = NULL;
  m_hMessageLoopThread = m_hPollingError = m_hRaceProtection = NULL;
}

//-------------------------------------------------------------------------------------
// Initializes internal variables
CHotkeyHandler::~CHotkeyHandler()
{
  if (m_bStarted)
    Stop();
}

//-------------------------------------------------------------------------------------
// Releases and deletes the protection mutex
void CHotkeyHandler::EndRaceProtection()
{
  if (m_hRaceProtection)
  {
    ::ReleaseMutex(m_hRaceProtection);
    ::CloseHandle(m_hRaceProtection);
    // invalidate handle
    m_hRaceProtection = NULL;
  }
}

//-------------------------------------------------------------------------------------
// Creates and acquires protection mutex
bool CHotkeyHandler::BeginRaceProtection()
{
  LPCTSTR szMutex = _T("{97B7C451-2467-4C3C-BB91-25AC918C2430}");
  // failed to create a mutex ? to open ?
  if (
      // failed to create and the object does not exist?
      (
       !(m_hRaceProtection = ::CreateMutex(NULL, FALSE, szMutex)) &&
        (::GetLastError() != ERROR_ALREADY_EXISTS)
      ) ||
      !(m_hRaceProtection = ::OpenMutex(MUTEX_ALL_ACCESS, FALSE, szMutex))
     )
    return false;
  ::WaitForSingleObject(m_hRaceProtection, INFINITE);
  return true;
}


//-------------------------------------------------------------------------------------
// removes a disabled hotkey from the internal list
// If this function is of use only before a Start() call or after a Stop()
int CHotkeyHandler::RemoveHandler(const int index)
{
  // index out of range ?
  if (m_listHk.size() <= index)
    return hkheNoEntry;

  // mark handler as deleted
  m_listHk.at(index).deleted = true;
  return hkheOk;
}

//-------------------------------------------------------------------------------------
// Generates a unique atom and then registers a hotkey
//
std::wstring CHotkeyHandler::GetErrMsg()
{
    LPWSTR lpMsgBuf;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&lpMsgBuf,
        0,
        NULL);

    std::wstring msg = lpMsgBuf;
    //MessageBox(NULL, (LPCTSTR)lpMsgBuf, TEXT("NamedPipe Error"), MB_OK | MB_ICONINFORMATION);
    LocalFree(lpMsgBuf);
    return msg;
}

int CHotkeyHandler::EnableHotkey(const int index)
{
  TCHAR atomname[MAX_PATH];
  ATOM a;

  // get hotkey definition
  tHotkeyDef *def = &m_listHk.at(index);

  // If entry does not exist then return Ok
  if (def->deleted)
    return hkheOk;

  // compose atom name
  wsprintf(atomname, _T("ED7D65EB-B139-44BB-B455-7BB83FE361DE-%08lX"), MAKELONG(def->virt, def->mod));

  // Try to create an atom
  a = ::GlobalAddAtom(atomname);

  // could not create? probably already there
  if (!a)
    a = ::GlobalFindAtom(atomname); // try to locate atom

  if (!a || !::RegisterHotKey(m_hWnd, a, def->mod, def->virt))
  {
    //MOD_CONTROL | MOD_ALT MOD_SHIFT
    wchar_t info[256] = {0};
    swprintf_s(info, 256, L"hotkey: Failed to RegisterHotKey %d(alt:1, ctrl:2, shift:4) %c: %s\n",
        def->mod, (char)def->virt,
        GetErrMsg().c_str());
    OutputDebugStringW(info);

    if (a)
      ::GlobalDeleteAtom(a);
    return hkheRegHotkeyError;
  }

  // store atom into definition too
  def->id = a;
  return hkheOk;
}


//-------------------------------------------------------------------------------------
// Unregisters a hotkey and deletes the atom
int CHotkeyHandler::DisableHotkey(const int index)
{
  // get hotkey definition
  tHotkeyDef *def = &m_listHk.at(index);

  // skip deleted entry
  if (def->deleted)
    return hkheOk;

  // already registered
  if (def->id)
  {
    UnregisterHotKey(m_hWnd, def->id);
    GlobalDeleteAtom(def->id);
    return hkheOk;
  }
  else
    return hkheNoEntry;
}


//-------------------------------------------------------------------------------------
// Locates a hotkey definition given its ID
int CHotkeyHandler::FindHandlerById(const ATOM id, tHotkeyDef *&def)
{
  tHotkeyList::iterator i;

  for (i=m_listHk.begin(); i != m_listHk.end(); i++)
  {
    def = &*i;
    if ((def->id == id) && !def->deleted)
      return hkheOk;
  }
  return hkheNoEntry;
}

//-------------------------------------------------------------------------------------
// Finds a deleted entry
int CHotkeyHandler::FindDeletedHandler(int &idx)
{
  tHotkeyDef *def;
  int i, c = m_listHk.size();
  for (i=0; i< c; i++)
  {
    def = &m_listHk[i];
    if (def->deleted)
    {
      idx = i;
      return hkheOk;
    }
  }
  return hkheNoEntry;
}

//-------------------------------------------------------------------------------------
// Finds a hotkeydef given it's modifier 'mod' and virtuak key 'virt'
// If return value is hkheOk then 'idx' is filled with the found index
// Otherwise 'idx' is left untouched.
int CHotkeyHandler::FindHandler(WORD mod, WORD virt, int &idx)
{
  tHotkeyDef *def;
  int i, c = m_listHk.size();
  for (i=0; i < c; i++)
  {
    def = &m_listHk[i];
    // found anything in the list ?
    if ( (def->mod == mod) && (def->virt == virt) && !def->deleted)
    {
      // return its id
      idx = i;
      return hkheOk;
    }
  }
  return hkheNoEntry;
}

//-------------------------------------------------------------------------------------
// Inserts a hotkey into the list
// Returns into 'idx' the index of where the definition is added
// You may use the returned idx to modify/delete this definition
int CHotkeyHandler::InsertHandler(WORD mod, WORD virt, tHotkeyCB cb, string param, int &idx)
{
  tHotkeyDef def;

  // Try to find a deleted entry and use it
  if (FindDeletedHandler(idx) == hkheOk)
  {
    tHotkeyDef *d = &m_listHk.at(idx);
    d->deleted = false;
    d->id = NULL;
    d->callback = cb;
    d->virt = virt;
    d->mod = mod;
    d->param = param;
  }
  // Add a new entry
  else if (FindHandler(mod, virt, idx) == hkheNoEntry)
  {
    def.mod = mod;
    def.virt = virt;
    def.callback = cb;
    def.id = NULL;
    def.deleted = false;
    def.param = param;
    idx = m_listHk.size();
    m_listHk.push_back(def);
  }
  return hkheOk;
}

//-------------------------------------------------------------------------------------
// Window Procedure
// Almost empty; it responds to the WM_DESTROY message only
LRESULT CALLBACK CHotkeyHandler::WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
  switch (Msg)
  {
    case WM_DESTROY:
      ::PostQuitMessage(0);
      break;
    default:
      return ::DefWindowProc(hWnd, Msg, wParam, lParam);
  }
  return 0;
}


//-------------------------------------------------------------------------------------
// This function is run from a new thread, it:
// 1) Create our hidden window to process hotkeys
// 2) Enables (registers) all defined hotkeys
// 3) Starts the message loop
DWORD WINAPI CHotkeyHandler::MessageLoop(LPVOID Param)
{
  int rc;

  OutputDebugStringA("hotkey, MessageLoop...");
  CHotkeyHandler *_this = reinterpret_cast<CHotkeyHandler *>(Param);

  // create window
  if ((rc = _this->MakeWindow()) != hkheOk)
  {
    _this->m_PollingError = rc;
    ::SetEvent(_this->m_hPollingError);
    OutputDebugStringA("hotkey, outer MessageLoop1...");
    return rc;
  }

  // register all hotkeys
  for (int i=0; i<_this->m_listHk.size(); i++)
  {
    // try to register
    if ((rc = _this->EnableHotkey(i)) != hkheOk)
    {
      // disable hotkeys enabled so far
      for (int j=0;j<i;j++)
        _this->DisableHotkey(j);

      // uninit window
      _this->MakeWindow(true);

      // signal that error is ready
      _this->m_PollingError = rc;
      ::SetEvent(_this->m_hPollingError);
      OutputDebugStringA("hotkey, outer MessageLoop because failed to EnableHotkey");
      return rc;
    }
  }

  _this->m_PollingError = hkheOk;
  ::SetEvent(_this->m_hPollingError);

  MSG msg;
  BOOL bRet;

  OutputDebugStringA("hotkey, GetMessage...");
  while ( ((bRet = ::GetMessage(&msg, _this->m_hWnd, 0, 0)) != 0) )
  {
    if (bRet == -1)
      break;

    // hotkey received ?
    if (msg.message == WM_HOTKEY)
    {
      tHotkeyDef *def;

      OutputDebugStringA("received a hotkey");
      // try to find handler (wParam == id (ATOM)
      if (_this->FindHandlerById( (ATOM) msg.wParam, def) == hkheOk)
      {
        // call the registered handler
          OutputDebugStringA("received a hotkey1");
        if (def->callback)
          //def->callback(_this->m_lpCallbackParam);
          OutputDebugStringA("received a hotkey12");
          def->callback((void*)def->param.c_str());
      }
    }
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }
  return hkheOk;
}

//-------------------------------------------------------------------------------------
// This asynchronous function will register all hotkeys and starts the window message
// loop into its own thread. You would have to call Stop() to unroll everything
//
int CHotkeyHandler::Start(LPVOID cbParam)
{
  int rc;

  // avoid starting again if already started
  if (m_bStarted)
    return hkheOk;

  // Do not start if no entries are there!
  if (m_listHk.empty())
    return hkheNoEntry;

  if (!BeginRaceProtection())
    return hkheInternal;

  if (!(m_hPollingError = ::CreateEvent(NULL, FALSE, FALSE, NULL)))
  {
    rc = hkheMessageLoop;
    goto cleanup;
  }

  m_lpCallbackParam = cbParam;

  // create message loop thread
  DWORD dwThreadId;
  m_hMessageLoopThread =
     ::CreateThread(NULL,
                    NULL,
                    MessageLoop,
                    static_cast<LPVOID>(this),
                    NULL,
                    &dwThreadId);

  if (!m_hMessageLoopThread)
  {
    rc = hkheMessageLoop;
    goto cleanup;
  }

  // wait until we get an error code from the message loop thread
  ::WaitForSingleObject(m_hPollingError, INFINITE);

  if (m_PollingError != hkheOk)
  {
    ::CloseHandle(m_hMessageLoopThread);
    m_hMessageLoopThread = NULL;
  }
  rc = m_PollingError;

  m_bStarted = true;
cleanup:
  EndRaceProtection();
  return rc;
}


//-------------------------------------------------------------------------------------
// Disables all hotkeys then kills the hidden window that processed the hotkeys
// The return code is the one returned from the MessageLoop()
//
int CHotkeyHandler::Stop()
{
  // not started? return Success
  if (!m_bStarted || !m_hMessageLoopThread)
    return hkheOk;

  if (!BeginRaceProtection())
    return hkheInternal;

  // disable all hotkeys
  for (int i=0;i<m_listHk.size();i++)
    DisableHotkey(i);

  // tell message loop to terminate
  ::SendMessage(m_hWnd, WM_DESTROY, 0, 0);

  // wait for the thread to exit by itself
  if (::WaitForSingleObject(m_hMessageLoopThread, 3000) == WAIT_TIMEOUT)
    ::TerminateThread(m_hMessageLoopThread, 0); // kill thread

  DWORD exitCode;
  ::GetExitCodeThread(m_hMessageLoopThread, &exitCode);

  // close handle of thread
  ::CloseHandle(m_hMessageLoopThread);
  // reset this variable
  m_hMessageLoopThread = NULL;

  // unregister window class
  MakeWindow(true);

  // kill error polling event
  ::CloseHandle(m_hPollingError);

  m_bStarted = false;

  EndRaceProtection();
  return exitCode;
}


//-------------------------------------------------------------------------------------
// Creates the hidden window that will receive hotkeys notification
// When bUnmake, it will Unregister the window class
int CHotkeyHandler::MakeWindow(bool bUnmake)
{
  HWND hwnd;
  WNDCLASSEX wcl;
  HINSTANCE hInstance = ::GetModuleHandle(NULL);

  // Our hotkey processing window class
  LPCTSTR szClassName = _T("HKWND-CLS-BC090410-3872-49E5-BDF7-1BB8056BF696");

  if (bUnmake)
  {
    UnregisterClass(szClassName, hInstance);
    m_hWnd = NULL;
    return hkheOk;
  }

  // Set the window class
  wcl.cbSize         = sizeof(WNDCLASSEX);
  wcl.cbClsExtra     = 0;
  wcl.cbWndExtra     = 0;
  wcl.hbrBackground  = NULL;//(HBRUSH) GetStockObject(WHITE_BRUSH);
  wcl.lpszClassName  = szClassName;
  wcl.lpszMenuName   = NULL;
  wcl.hCursor        = NULL;//LoadCursor(NULL, IDC_ARROW);
  wcl.hIcon          = NULL;//LoadIcon(NULL, IDI_APPLICATION);
  wcl.hIconSm        = NULL;//LoadIcon(NULL, IDI_WINLOGO);
  wcl.hInstance      = hInstance;
  wcl.lpfnWndProc    = WindowProc;
  wcl.style          = 0;

  // Failed to register class other than that the class already exists?
  if (!::RegisterClassEx(&wcl) && (::GetLastError() != ERROR_CLASS_ALREADY_EXISTS))
    return hkheClassError;

  // Create the window
  hwnd = ::CreateWindowEx(
                      0,
                      szClassName,
                      _T("CHotkeyHandlerWindow"),
                      WS_OVERLAPPEDWINDOW,
                      0,
                      0,
                      100,
                      100,
                      HWND_DESKTOP,
                      NULL,
                      hInstance,
                      NULL);

  // Window creation failed ?
  if (!hwnd)
    return hkheWindowError;

  // hide window
  ::ShowWindow(hwnd, bDebug ? SW_SHOW : SW_HIDE);

  m_hWnd = hwnd;
  return hkheOk;
}



//---------------------------------------------------------------------------------
// Converts HOTKEYFLAGS used by CHotKeyCtrl into MOD_XXX used by windows API
//
WORD CHotkeyHandler::HotkeyFlagsToModifiers(WORD hkf)
{
  WORD r;

  r = ((hkf & HOTKEYF_CONTROL) ? MOD_CONTROL : 0) |
      ((hkf & HOTKEYF_ALT)     ? MOD_ALT : 0)     |
      ((hkf & HOTKEYF_SHIFT)   ? MOD_SHIFT : 0);
  return r;
}

//---------------------------------------------------------------------------------
// Converts from MOD_XXX modifiers into HOTKEYF_XXXX
//
WORD CHotkeyHandler::HotkeyModifiersToFlags(WORD modf)
{
  WORD r;

  r = ((modf & MOD_CONTROL) ? HOTKEYF_CONTROL : 0) |
      ((modf & MOD_ALT)     ? HOTKEYF_ALT : 0)     |
      ((modf & MOD_SHIFT)   ? HOTKEYF_SHIFT : 0);
  return r;
}