// stdafx.h : ���� ��������� ���� ��������� �ʴ�
// ǥ�� �ý��� ���� ���� �� ������Ʈ ���� ���� ������
// ��� �ִ� ���� �����Դϴ�.
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <mstcpip.h>
#include <process.h>
#include <strsafe.h>
#include <mmsystem.h>
#include <conio.h>
#include <float.h>
#include <time.h>

#include <psapi.h>
#include <dbghelp.h>
#include <crtdbg.h>
#include <tlhelp32.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Ws2_32.lib")

#pragma comment(lib, "DbgHelp.Lib")
#pragma comment(lib, "ImageHlp")
#pragma comment(lib, "psapi")

#define		PROFILE_CHECK

// TODO: ���α׷��� �ʿ��� �߰� ����� ���⿡�� �����մϴ�.
#include "APIHook.h"
#include "CrashDump.h"
#include "ProFiler.h"
#include "MemoryPool.h"
#include "LockfreeStack.h"
#include "LockfreeQueue.h"
#include "AyaStreamSQ.h"
#include "NPacket.h"
#include "ArrayStack.h"
#include "Session.h"
#include "NetServer.h"