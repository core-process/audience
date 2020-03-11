#pragma once

#include <windows.h>

static bool co_wait_for_object(HANDLE handle)
{
  while (true)
  {
    DWORD result = MsgWaitForMultipleObjectsEx(
        1,
        &handle,
        INFINITE,
        QS_ALLEVENTS,
        MWMO_INPUTAVAILABLE);

    if (WAIT_FAILED == result)
    {
      return false;
    }
    else if (WAIT_OBJECT_0 == result)
    {
      return true;
    }
    else if ((WAIT_OBJECT_0 + 1) == result)
    {
      MSG msg = {0};

      while (PeekMessageW(&msg,
                          0, // any window
                          0, // all messages
                          0, // all messages
                          PM_REMOVE))
      {
        if (WM_QUIT == msg.message)
        {
          return false;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
    }
    else
    {
      return false;
    }
  }
}
