#ifndef __HOTKEYHANDLER__INC_
#define __HOTKEYHANDLER__INC_

/*
 -----------------------------------------------------------------------------
 Check implementation file for copyright information and for this library usage info.
 -----------------------------------------------------------------------------
*/

#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <vector>
#include <string>
using namespace std;

class CHotkeyHandler
{
private:
  // on hotkey occurence callback
  typedef void (*tHotkeyCB)(void *);

  // hotkey definition
  typedef struct
  {
    WORD mod;
    WORD virt;
    tHotkeyCB callback;
    ATOM id;
    string param; //Yage add this member
    bool deleted;
  } tHotkeyDef;

  // hotkeys definition list
  typedef std::vector<tHotkeyDef> tHotkeyList;

  // hotkey list
  tHotkeyList m_listHk;

  // window call back
  static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);

  // our hotkey processing window
  HWND m_hWnd;

  // register window class and creates the window
  int MakeWindow(bool bUnmake = false);

  //
  static DWORD WINAPI MessageLoop(LPVOID Param);

  //
  std::wstring GetErrMsg();
  int EnableHotkey(const int);
  int DisableHotkey(const int);

  // Finds the unique ID (ATOM) of an already enabled hotkey
  int FindHandlerById(const ATOM id, tHotkeyDef *&);

  // Finds the index of an already inserted Hotkey def by Mod&Virt
  int FindHandler(WORD mod, WORD virt, int &index);

  // Finds for a deleted entry
  int FindDeletedHandler(int &idx);

  // handle of the message loop thread
  HANDLE m_hMessageLoopThread;

  // Error code that I poll from inside the MessageLoop()
  int    m_PollingError;
  HANDLE m_hPollingError;

  // Parameter passed to the callback of every hotkey
  LPVOID m_lpCallbackParam;

  // Race condition prevention mutex
  HANDLE m_hRaceProtection;
  bool   BeginRaceProtection();
  void   EndRaceProtection();

  bool   m_bStarted;
public:
  bool bDebug;
  CHotkeyHandler(bool Debug = false);
  ~CHotkeyHandler();

  int Start(LPVOID = NULL);
  int Stop();

  // Inserts a hotkey definition
  int InsertHandler(WORD mod, WORD virt, tHotkeyCB cb, string param, int &index);

  // Removes a hotkey definition
  int RemoveHandler(const int index);

  static WORD HotkeyModifiersToFlags(WORD modf);
  static WORD HotkeyFlagsToModifiers(WORD hkf);

  //HotKeyHandlerErrorsXXXX
  enum {hkheOk = 0,  // success
        hkheClassError, // window class registration error
        hkheWindowError, // window creation error
        hkheNoEntry, // No handler found at given index
        hkheRegHotkeyError, // could not register hotkey
        hkheMessageLoop, // could not create message loop thread
        hkheInternal // Internal error
       };
};

#endif