#include <conio.h>
#include <stdio.h>
#include "hotkeyhandler.h"

void handA(void *)
{
  printf("this is A\n");
}

void handQ(void *)
{
  printf("this is Q\n");
}

void hand1(void *s)
{
  WinExec((char *)s, SW_SHOW);
}

int main(void)
{
  int err, id;

  CHotkeyHandler hk;

  hk.RemoveHandler(id = 0);

  hk.InsertHandler(MOD_CONTROL | MOD_ALT, 'Q', handQ, id);
  hk.InsertHandler(MOD_CONTROL | MOD_ALT, 'A', handA, id);
  hk.InsertHandler(MOD_CONTROL | MOD_ALT, '1', hand1, id);

  err = hk.Start("calc.exe");
  if (err != CHotkeyHandler::hkheOk)
  {
    printf("Error %d on Start()\n", err);
    return err;
  }

  printf("hotkeys started!!!\n...press any key to stop them...\n");
  getch();
  err = hk.Stop();
  return 0;
}