#include "stdafx.h"

//-----------------------------------------------------------------------------------------
// ������, �Ҹ���
//-----------------------------------------------------------------------------------------
CNetServer::CNetServer()
{
	CCrashDump::CCrashDump();
	ProfileInit();

	if (!CNPacket::_ValueSizeCheck())
		CCrashDump::Crash();

	_pBlankStack = new CLockfreeStack<int>();

	///////////////////////////////////////////////////////////////////////////////
	// �� ���� ����
	///////////////////////////////////////////////////////////////////////////////
	for (int iCnt = eMAX_SESSION - 1; iCnt >= 0; iCnt--)
	{
		_Session[iCnt] = new SESSION;
		///////////////////////////////////////////////////////////////////////////
		// ���� ���� ����ü �ʱ�ȭ
		///////////////////////////////////////////////////////////////////////////
		_Session[iCnt]->_SessionInfo._Socket = INVALID_SOCKET;
		memset(&_Session[iCnt]->_SessionInfo._wIP, 0, sizeof(_Session[iCnt]->_SessionInfo._wIP));
		_Session[iCnt]->_SessionInfo._iPort = 0;

		///////////////////////////////////////////////////////////////////////////
		// ���� �ʱ�ȭ
		///////////////////////////////////////////////////////////////////////////
		_Session[iCnt]->_iSessionID = -1;

		memset(&_Session[iCnt]->_SendOverlapped, 0, sizeof(OVERLAPPED));
		memset(&_Session[iCnt]->_RecvOverlapped, 0, sizeof(OVERLAPPED));

		_Session[iCnt]->_RecvQ.ClearBuffer();
		_Session[iCnt]->_SendQ.ClearBuffer();

		_Session[iCnt]->_bSendFlag = false;

		_Session[iCnt]->_IOBlock = (IOBlock *)_aligned_malloc(sizeof(IOBlock), 16);

		_Session[iCnt]->_IOBlock->_iIOCount = 0;
		_Session[iCnt]->_IOBlock->_iReleaseFlag = false;

		memset(_Session[iCnt]->_pSentPacket, 0, sizeof(_Session[iCnt]->_pSentPacket));
		_Session[iCnt]->_lSentPacketCnt = 0;

		InsertBlankSessionIndex(iCnt);
	}

	
	///////////////////////////////////////////////////////////////////////////////
	// LanServer ���� ����
	///////////////////////////////////////////////////////////////////////////////
	_iSessionID = 0;
}

CNetServer::~CNetServer()
{

}


//-----------------------------------------------------------------------------------------
// ���� ����
//-----------------------------------------------------------------------------------------
bool				CNetServer::Start(WCHAR* wOpenIP, int iPort, int iWorkerThreadNum, bool bNagle, int iMaxConnect)
{
	int				result;
	unsigned long	ul = 1;


	///////////////////////////////////////////////////////////////////////////////////////
	// ���� �ʱ�ȭ
	///////////////////////////////////////////////////////////////////////////////////////
	WSADATA wsa;
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsa))
		return false;

	///////////////////////////////////////////////////////////////////////////////////////
	// IO Completion Port ����
	///////////////////////////////////////////////////////////////////////////////////////
	_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (NULL == _hIOCP)
		return false;

	///////////////////////////////////////////////////////////////////////////////////////
	// �������� ����
	///////////////////////////////////////////////////////////////////////////////////////
	_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == _ListenSocket)
		return false;

	///////////////////////////////////////////////////////////////////////////////////////
	// bind
	///////////////////////////////////////////////////////////////////////////////////////
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(iPort);
	InetPton(AF_INET, wOpenIP, &serverAddr.sin_addr);
	result = bind(_ListenSocket, (SOCKADDR *)&serverAddr, sizeof(SOCKADDR_IN));
	if (SOCKET_ERROR == result)
		return false;

	///////////////////////////////////////////////////////////////////////////////////
	// ���� Send Buffer ������ 0���� ����
	///////////////////////////////////////////////////////////////////////////////////
	int optval;
	setsockopt(_ListenSocket, SOL_SOCKET, SO_SNDBUF, (char*)&optval, 0);
		
	///////////////////////////////////////////////////////////////////////////////////////
	// listen
	///////////////////////////////////////////////////////////////////////////////////////
	result = listen(_ListenSocket, SOMAXCONN);
	if (SOCKET_ERROR == result)
		return false;

	///////////////////////////////////////////////////////////////////////////////////////
	// nagle option
	///////////////////////////////////////////////////////////////////////////////////////
	_bNagle = bNagle;


	///////////////////////////////////////////////////////////////////////////////////////
	// Thread ����
	///////////////////////////////////////////////////////////////////////////////////////
	DWORD dwThreadID;

	_hAcceptThread = (HANDLE)_beginthreadex(
		NULL,
		0,
		AcceptThread,
		this,
		0,
		(unsigned int *)&dwThreadID
		);

	_hMonitorThread = (HANDLE)_beginthreadex(
		NULL,
		0,
		MonitorThread,
		this,
		0,
		(unsigned int *)&dwThreadID
		);

	if (iWorkerThreadNum > eMAX_THREAD)
		return false;

	_iWorkerThreadNum = iWorkerThreadNum;

	for (int iCnt = 0; iCnt < iWorkerThreadNum; iCnt++)
	{
		_hWorkerThread[iCnt] = (HANDLE)_beginthreadex(
			NULL,
			0,
			WorkerThread,
			this,
			0,
			(unsigned int *)&dwThreadID
			);
	}

	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////
// ���� ����
///////////////////////////////////////////////////////////////////////////////////////////
void				CNetServer::Stop()
{
	
}


///////////////////////////////////////////////////////////////////////////////////////////
// ���� ����
///////////////////////////////////////////////////////////////////////////////////////////
bool				CNetServer::SendPacket(__int64 iSessionID, CNPacket *pPacket)
{
	SESSION *pSession = SessionGetLock(iSessionID);

	if (nullptr != pSession)
	{
		pPacket->Encode();

		PRO_BEGIN(L"Packet addref");
		pPacket->addRef();
		PRO_END(L"Packet addref");

		PRO_BEGIN(L"PacketQueue Put");
		pSession->_SendQ.Put(pPacket);
		PRO_END(L"PacketQueue Put");

		PRO_BEGIN(L"SendPost");
		SendPost(pSession);
		PRO_END(L"SendPost");	

		InterlockedIncrement((LONG *)&_lSendPacketCounter);
	}

	SessionGetUnlock(pSession);

	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////
// ���� ����
///////////////////////////////////////////////////////////////////////////////////////////
bool				CNetServer::Disconnect(__int64 iSessionID)
{
	int			iSessionIndex = GET_SESSIONINDEX(iSessionID);
	
	DisconnectSession(_Session[iSessionIndex]);
}

///////////////////////////////////////////////////////////////////////////////////////////
// ���� �����ϴ� ������ �κ�
///////////////////////////////////////////////////////////////////////////////////////////
int					CNetServer::AccpetThread_update()
{
	HANDLE			hResult;

	SOCKET			ClientSocket;
	SOCKADDR_IN		ClientAddr;
	int				iAddrLen = sizeof(SOCKADDR_IN);
	

	SESSIONINFO		SessionInfo;

	int				iBlankIndex;


	while (1)
	{
		///////////////////////////////////////////////////////////////////////////////////
		// accpet
		///////////////////////////////////////////////////////////////////////////////////
		ClientSocket = accept(_ListenSocket, (SOCKADDR *)&ClientAddr, &iAddrLen);
		if (INVALID_SOCKET == ClientSocket)
			CCrashDump::Crash();


		InterlockedIncrement((LONG *)&_lAcceptCounter);
		InterlockedIncrement((LONG *)&_lAcceptTotalCounter);

		///////////////////////////////////////////////////////////////////////////////////
		// ���� ���� ���� ����
		///////////////////////////////////////////////////////////////////////////////////
		SessionInfo._Socket = ClientSocket;
		InetNtop(AF_INET, &ClientAddr.sin_addr, SessionInfo._wIP, 16);
		SessionInfo._iPort = ntohs(ClientAddr.sin_port);

		iBlankIndex = GetBlankSessionIndex();

		///////////////////////////////////////////////////////////////////////////////////
		// �ִ� ���� �ʰ���
		///////////////////////////////////////////////////////////////////////////////////
		if (iBlankIndex < 0)
		{
			closesocket(ClientSocket);
			continue;
		}

		///////////////////////////////////////////////////////////////////////////////////
		// ���� ��û(White IP�� �����ϰ� �ϱ� ��)
		///////////////////////////////////////////////////////////////////////////////////
		if (!OnConnectionRequest(&SessionInfo))
			continue;
		
		///////////////////////////////////////////////////////////////////////////////////
		// keepalive �ɼ�
		//
		// onoff			 -> keepalive ���� ����(0�� �ƴϸ� ����)
		// keepalivetime	 -> ù keepalive ��Ŷ�� ���� �������� �ð�
		// keepaliveinterval -> ������ ������� ���� keepalive ��Ŷ�� ���۵Ǵ� ���� 
		///////////////////////////////////////////////////////////////////////////////////
		tcp_keepalive tcpkl;

		tcpkl.onoff = 1;
		tcpkl.keepalivetime = 30000;
		tcpkl.keepaliveinterval = 2000;

		DWORD dwReturnByte;
		WSAIoctl(SessionInfo._Socket, SIO_KEEPALIVE_VALS, &tcpkl, sizeof(tcp_keepalive),
			0, 0, &dwReturnByte, NULL, NULL);

		///////////////////////////////////////////////////////////////////////////////////
		// nagle �ɼ�
		///////////////////////////////////////////////////////////////////////////////////
		if (_bNagle)
		{
			char opt_val = true;
			setsockopt(SessionInfo._Socket, IPPROTO_TCP, TCP_NODELAY, &opt_val, sizeof(opt_val));
		}

		///////////////////////////////////////////////////////////////////////////////////
		// �� ���� ã�� ���� ����
		///////////////////////////////////////////////////////////////////////////////////
		_Session[iBlankIndex]->_SessionInfo = SessionInfo;

		///////////////////////////////////////////////////////////////////////////////////
		// ���� ID �����ؼ� �����
		///////////////////////////////////////////////////////////////////////////////////
		int iSessionID = InterlockedIncrement((LONG *)&_iSessionID);
		_Session[iBlankIndex]->_iSessionID = COMBINE_ID_WITH_INDEX(iSessionID, iBlankIndex);

		/////////////////////////////////////////////////////////////////////
		// IOCP ���
		/////////////////////////////////////////////////////////////////////
		hResult = CreateIoCompletionPort((HANDLE)_Session[iBlankIndex]->_SessionInfo._Socket,
			_hIOCP,
			(ULONG_PTR)_Session[iBlankIndex],
			0);
		if (!hResult)
			PostQueuedCompletionStatus(_hIOCP, 0, 0, 0);

		/////////////////////////////////////////////////////////////////////
		// OnClientJoin
		// �������ʿ� ������ �������� �˸�
		// �α��� ��Ŷ ������ �߿� ���� �� ������ IOCount�� �̸� �÷��ش�
		/////////////////////////////////////////////////////////////////////
		InterlockedIncrement64((LONG64 *)&_Session[iBlankIndex]->_IOBlock->_iIOCount);
		OnClientJoin(&_Session[iBlankIndex]->_SessionInfo, _Session[iBlankIndex]->_iSessionID);

		InterlockedIncrement((long *)&_lSessionCount);

		PRO_BEGIN(L"RecvPost - AccpetTH");
		RecvPost(_Session[iBlankIndex], true);
		PRO_END(L"RecvPost - AccpetTH");
	}

	return 0;
}

int					CNetServer::WorkerThread_update()
{
	int				result;

	OVERLAPPED		*pOverlapped;
	SESSION			*pSession;
	DWORD			dwTransferred;

	while (1)
	{
		pOverlapped		= NULL;
		pSession		= NULL;
		dwTransferred	= 0;

		PRO_BEGIN(L"GQCS IOComplete");
		result = GetQueuedCompletionStatus(
			_hIOCP,
			&dwTransferred,
			(PULONG_PTR)&pSession,
			&pOverlapped,
			INFINITE);
		PRO_END(L"GQCS IOComplete");

		OnWorkerThreadBegin();

		///////////////////////////////////////////////////////////////////////////////////
		// Error, ���� ó��
		///////////////////////////////////////////////////////////////////////////////////
		// IOCP ���� ���� ����
		if (result == FALSE && (pOverlapped == NULL || pSession == NULL))
		{
			int iErrorCode = WSAGetLastError();
			OnError(iErrorCode, L"IOCP HANDLE Error\n");

			break;
		}

		// ��Ŀ������ ���� ����
		else if (dwTransferred == 0 && pSession == NULL && pOverlapped == NULL)
		{
			OnError(0, L"Worker Thread Done.\n");
			PostQueuedCompletionStatus(_hIOCP, 0, NULL, NULL);
			return 0;
		}

		//----------------------------------------------------------------------------
		// ��������
		// Ŭ���̾�Ʈ ���� closesocket() Ȥ�� shutdown() �Լ��� ȣ���� ����
		//----------------------------------------------------------------------------
		else if (dwTransferred == 0)
		{
			DisconnectSession(pSession);
		}
		//----------------------------------------------------------------------------

		if (pOverlapped == &(pSession->_RecvOverlapped))
		{
			PRO_BEGIN(L"CompleteRecv");
			CompleteRecv(pSession, dwTransferred);
			PRO_END(L"CompleteRecv");
		}

		if (pOverlapped == &(pSession->_SendOverlapped))
		{
			PRO_BEGIN(L"CompleteSend");
			CompleteSend(pSession, dwTransferred);
			PRO_END(L"CompleteSend");
		}

		if (0 == InterlockedDecrement64((LONG64 *)&pSession->_IOBlock->_iIOCount))
			ReleaseSession(pSession);

		OnWorkerThreadEnd();
	}

	return true;
}

int					CNetServer::MonitorThread_update()
{
	timeBeginPeriod(1);

	while (1)
	{
		_lAcceptTPS = _lAcceptCounter;
		_lAcceptTotalTPS += _lAcceptTotalCounter;
		_lRecvPacketTPS = _lRecvPacketCounter;
		_lSendPacketTPS = _lSendPacketCounter;
		_lPacketPoolTPS = CNPacket::GetPacketCount();

		_lAcceptCounter = 0;
		_lAcceptTotalCounter = 0;
		_lRecvPacketCounter = 0;
		_lSendPacketCounter = 0;

		Sleep(999);
	}

	timeEndPeriod(1);

	return 0;
}



///////////////////////////////////////////////////////////////////////////////////////////
// Recv, Send ���
///////////////////////////////////////////////////////////////////////////////////////////
void				CNetServer::RecvPost(SESSION *pSession, bool bAcceptRecv)
{
	int result, iCount = 1;
	DWORD dwRecvSize, dwFlag = 0;
	WSABUF wBuf[2];

	///////////////////////////////////////////////////////////////////////////////////////
	// WSABUF ���
	///////////////////////////////////////////////////////////////////////////////////////
	wBuf[0].buf = pSession->_RecvQ.GetWriteBufferPtr();
	wBuf[0].len = pSession->_RecvQ.GetNotBrokenPutSize();

	///////////////////////////////////////////////////////////////////////////////////////
	// ������ �������� ��� ���� ���� ���
	///////////////////////////////////////////////////////////////////////////////////////
	if (pSession->_RecvQ.GetFreeSize() > pSession->_RecvQ.GetNotBrokenPutSize())
	{
		wBuf[1].buf = pSession->_RecvQ.GetBufferPtr();
		wBuf[1].len = pSession->_RecvQ.GetFreeSize() -
						pSession->_RecvQ.GetNotBrokenPutSize();

		iCount++;
	}
	
	memset(&pSession->_RecvOverlapped, 0, sizeof(OVERLAPPED));

	///////////////////////////////////////////////////////////////////////////////////////
	// ù ���ӽ��� Recv�� IOCount�� �ø��� ����(�α��� ��Ŷ ����)
	///////////////////////////////////////////////////////////////////////////////////////
	if (!bAcceptRecv)
		InterlockedIncrement64((LONG64 *)&pSession->_IOBlock->_iIOCount);

	PRO_BEGIN(L"WSARecv");
	result = WSARecv(
		pSession->_SessionInfo._Socket, 
		wBuf,
		iCount, 
		&dwRecvSize,
		&dwFlag,
		&pSession->_RecvOverlapped, 
		NULL
		);

	if (result == SOCKET_ERROR)
	{
		int iErrorCode = GetLastError();
		///////////////////////////////////////////////////////////////////////////////////
		// WSA_IO_PENDING -> Overlapped ������ �غ�Ǿ����� �Ϸ���� ���� ���
		// �� �ܿ��� ������ ��
		///////////////////////////////////////////////////////////////////////////////////
		if (iErrorCode != WSA_IO_PENDING)
		{
			///////////////////////////////////////////////////////////////////////////////
			// 10054 : Ŭ���̾�Ʈ �ʿ��� ������ ������ ���
			// 10058 : �ش� ������ shutdown�� ���
			///////////////////////////////////////////////////////////////////////////////
			if ((WSAECONNABORTED != iErrorCode) &&
				(WSAECONNRESET != iErrorCode) &&
				(WSAESHUTDOWN != iErrorCode))
				CCrashDump::Crash();


			if (0 == InterlockedDecrement64((LONG64 *)&pSession->_IOBlock->_iIOCount))
				ReleaseSession(pSession);
		}
	}
	PRO_END(L"WSARecv");
}

bool				CNetServer::SendPost(SESSION *pSession)
{
	int result, iCount = 0;
	DWORD dwSendSize, dwFlag = 0;
	WSABUF wBuf[eMAX_WSABUF];

	do
	{
		///////////////////////////////////////////////////////////////////////////////////
		// SendFlag Ȯ�� �� ����
		///////////////////////////////////////////////////////////////////////////////////
		bool bSendFlag = InterlockedCompareExchange((long *)&pSession->_bSendFlag, true, false);
		if (true == bSendFlag)
			break;
		
		///////////////////////////////////////////////////////////////////////////////////
		// SendQ ������ ����
		///////////////////////////////////////////////////////////////////////////////////
		int iSendQUseSize = pSession->_SendQ.GetUseSize();

		///////////////////////////////////////////////////////////////////////////////////
		// SendQ������ �ٽ� Ȯ��
		///////////////////////////////////////////////////////////////////////////////////
		if (0 == iSendQUseSize)
		{
			if (false == InterlockedCompareExchange((long *)&pSession->_bSendFlag, false, true))
				CCrashDump::Crash();

			///////////////////////////////////////////////////////////////////////////////
			// SendQ�� ����� ������(���� ������ �ٸ� ��)
			///////////////////////////////////////////////////////////////////////////////
			if (!pSession->_SendQ.isEmpty())
				continue;

			break;
		}

		if (eMAX_WSABUF <= iCount)
			iSendQUseSize = eMAX_WSABUF;
		

		///////////////////////////////////////////////////////////////////////////////////
		// WSABUF�� ��Ŷ �ֱ�
		///////////////////////////////////////////////////////////////////////////////////
		CNPacket *pPacket = nullptr;
		while (0 != pSession->_SendQ.GetUseSize())
		{
			if (!pSession->_SendQ.Get(&pPacket))
				break;

			wBuf[iCount].buf = (char *)pPacket->GetBufferHeaderPtr();
			wBuf[iCount].len = pPacket->GetDataSize();

			pSession->_pSentPacket[iCount] = (char *)pPacket;

			iCount++;
		}

		pSession->_lSentPacketCnt += iCount;

		memset(&pSession->_SendOverlapped, 0, sizeof(OVERLAPPED));

		InterlockedIncrement64((LONG64 *)&pSession->_IOBlock->_iIOCount);

		PRO_BEGIN(L"WSASend");
		result = WSASend(
			pSession->_SessionInfo._Socket,
			wBuf,
			iCount,
			&dwSendSize,
			dwFlag,
			&pSession->_SendOverlapped,
			NULL
			);

		if (result == SOCKET_ERROR)
		{
			int iErrorCode = WSAGetLastError();
			///////////////////////////////////////////////////////////////////////////////////
			// WSA_IO_PENDING -> Overlapped ������ �غ�Ǿ����� �Ϸ���� ���� ���
			// �� �ܿ��� ������ ��
			///////////////////////////////////////////////////////////////////////////////////
			if (iErrorCode != WSA_IO_PENDING)
			{
				// �ý��� �α� �����
				if (WSAENOBUFS == iErrorCode)
					CCrashDump::Crash();
				
				///////////////////////////////////////////////////////////////////////////////
				// Ŭ���̾�Ʈ �ʿ��� ������ ������ ���
				///////////////////////////////////////////////////////////////////////////////
				if ((WSAECONNABORTED != iErrorCode) &&
					(WSAECONNRESET != iErrorCode) &&
					(WSAESHUTDOWN != iErrorCode))
					CCrashDump::Crash();

				if (0 == InterlockedDecrement64((LONG64 *)&pSession->_IOBlock->_iIOCount))
					ReleaseSession(pSession);
			}
		}
		PRO_END(L"WSASend");
	} while (0);

	return true;
}



///////////////////////////////////////////////////////////////////////////////////////////
// Recv, Send ó��
///////////////////////////////////////////////////////////////////////////////////////////
bool				CNetServer::CompleteRecv(SESSION *pSession, DWORD dwTransferred)
{
	CNPacket::EncodeHeader header;

	//////////////////////////////////////////////////////////////////////////////
	// RecvQ WritePos �̵�(���� ��ŭ)
	//////////////////////////////////////////////////////////////////////////////
	if (dwTransferred != pSession->_RecvQ.MoveWritePos(dwTransferred))
		CCrashDump::Crash();

	PRO_BEGIN(L"Packet Alloc");
	CNPacket *pRecvPacket = CNPacket::Alloc();
	PRO_END(L"Packet Alloc");

	while (pSession->_RecvQ.GetUseSize() > 0)
	{

		PRO_BEGIN(L"Recv BufferDeque");
		//////////////////////////////////////////////////////////////////////////
		// RecvQ�� ��� ���̸�ŭ �ִ��� �˻� �� ������ Peek
		//////////////////////////////////////////////////////////////////////////
		if (pSession->_RecvQ.GetUseSize() <= CNPacket::eBUFFER_HEADER_SIZE)
			break;
		pSession->_RecvQ.Peek((char *)&header, CNPacket::eBUFFER_HEADER_SIZE);

		//////////////////////////////////////////////////////////////////////////
		// RecvQ�� ��� ���� + Payload ��ŭ �ִ��� �˻� �� ��� ����
		//////////////////////////////////////////////////////////////////////////
		if (pSession->_RecvQ.GetUseSize() < CNPacket::eBUFFER_HEADER_SIZE + header.shPacketLen)
			break;;
		pRecvPacket->SetHeader((char *)pSession->_RecvQ.GetReadBufferPtr());
		pSession->_RecvQ.RemoveData(CNPacket::eBUFFER_HEADER_SIZE);

		//////////////////////////////////////////////////////////////////////////
		// Payload�� ���� �� ��Ŷ Ŭ������ ����
		//////////////////////////////////////////////////////////////////////////
		pRecvPacket->PutData((unsigned char *)pSession->_RecvQ.GetReadBufferPtr(), header.shPacketLen);
		pSession->_RecvQ.RemoveData(header.shPacketLen);
		PRO_END(L"Recv BufferDeque");

		//////////////////////////////////////////////////////////////////////////
		// ��Ŷ ��ȣȭ
		//////////////////////////////////////////////////////////////////////////
		pRecvPacket->Decode();

		//////////////////////////////////////////////////////////////////////////
		// OnRecv ȣ��
		//////////////////////////////////////////////////////////////////////////
		PRO_BEGIN(L"OnRecv");
		OnRecv(pSession->_iSessionID, pRecvPacket);
		PRO_END(L"OnRecv");

		InterlockedIncrement((LONG *)&_lRecvPacketCounter);
	}

	PRO_BEGIN(L"Packet Free");
	pRecvPacket->Free();
	PRO_END(L"Packet Free");

	PRO_BEGIN(L"RecvPost");
	RecvPost(pSession);
	PRO_END(L"RecvPost");

	return true;
}

bool				CNetServer::CompleteSend(SESSION *pSession, DWORD dwTransferred)
{
	int			iSentCnt;

	//////////////////////////////////////////////////////////////////////////////
	// ������ �Ϸ�� ������ ����
	//////////////////////////////////////////////////////////////////////////////
	PRO_BEGIN(L"SentPacket Remove");
	for (iSentCnt = 0; iSentCnt < pSession->_lSentPacketCnt; iSentCnt++)
		((CNPacket *)pSession->_pSentPacket[iSentCnt])->Free();

	pSession->_lSentPacketCnt -= iSentCnt;

	PRO_END(L"SentPacket Remove");

	//////////////////////////////////////////////////////////////////////////////
	// �� ���´ٰ� Flag ��ȯ
	//////////////////////////////////////////////////////////////////////////////
	if (false == InterlockedCompareExchange((long *)&pSession->_bSendFlag, false, true))
		CCrashDump::Crash();

	PRO_BEGIN(L"SendPost - WorkerTh");
	//////////////////////////////////////////////////////////////////////////////
	// ������ ���������� �ٽ� ���
	//////////////////////////////////////////////////////////////////////////////
	if (!pSession->_SendQ.isEmpty())
		SendPost(pSession);

	PRO_END(L"SendPost - WorkerTh");

	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////
// ���� ����ȭ		 ->		Disconnect, SendPacket
///////////////////////////////////////////////////////////////////////////////////////////
SESSION*			CNetServer::SessionGetLock(__int64 iSessionID)
{
	int iSessionIndex = GET_SESSIONINDEX(iSessionID);

	///////////////////////////////////////////////////////////////////////////////////////
	// ������ ���� �ʰ� Ȯ��
	///////////////////////////////////////////////////////////////////////////////////////
	if (1 == InterlockedIncrement64((LONG64 *)&_Session[iSessionIndex]->_IOBlock->_iIOCount))
	{
		if (0 == InterlockedDecrement64((LONG64 *)&_Session[iSessionIndex]->_IOBlock->_iIOCount))
			ReleaseSession(_Session[iSessionIndex]);

		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// ������ �ٲ������ �ٽ� Ȯ��
	//////////////////////////////////////////////////////////////////////////////////////
	if (iSessionID != _Session[iSessionIndex]->_iSessionID)
	{
		if (0 == InterlockedDecrement64((LONG64 *)&_Session[iSessionIndex]->_IOBlock->_iIOCount))
			ReleaseSession(_Session[iSessionIndex]);

		return nullptr;
	}

	return _Session[iSessionIndex];
}

void				CNetServer::SessionGetUnlock(SESSION *pSession)
{
	///////////////////////////////////////////////////////////////////////////////////////
	// �ٽ� �۾� ī��Ʈ ������
	///////////////////////////////////////////////////////////////////////////////////////
	if (0 == InterlockedDecrement64((LONG64 *)&pSession->_IOBlock->_iIOCount))
		ReleaseSession(pSession);
}



///////////////////////////////////////////////////////////////////////////////////////////
// �� ���� ���
///////////////////////////////////////////////////////////////////////////////////////////
int					CNetServer::GetBlankSessionIndex()
{
	int iBlankIndex;

	if (_pBlankStack->isEmpty())
		iBlankIndex = -1;
	else
		_pBlankStack->Pop(&iBlankIndex);

	return iBlankIndex;
}

///////////////////////////////////////////////////////////////////////////////////////////
// ���� �ݳ�
///////////////////////////////////////////////////////////////////////////////////////////
void				CNetServer::InsertBlankSessionIndex(int iSessionIndex)
{
	_pBlankStack->Push(iSessionIndex);
}


///////////////////////////////////////////////////////////////////////////////////////////
// Disconnection
///////////////////////////////////////////////////////////////////////////////////////////
void				CNetServer::DisconnectSession(SESSION *pSession)
{
	CloseSocket(pSession->_SessionInfo._Socket);
}

///////////////////////////////////////////////////////////////////////////////////////////
// ���� ���� ����
///////////////////////////////////////////////////////////////////////////////////////////
void				CNetServer::CloseSocket(SOCKET socket)
{
	shutdown(socket, SD_BOTH);
}

///////////////////////////////////////////////////////////////////////////////////////////
// Release
///////////////////////////////////////////////////////////////////////////////////////////
void				CNetServer::ReleaseSession(SESSION *pSession)
{
	IOBlock stCompareBlock;

	stCompareBlock._iIOCount = 0;
	stCompareBlock._iReleaseFlag = 0;

	///////////////////////////////////////////////////////////////////////////////////////
	// ��¥ ������ �ؾ��ϴ� ������� �˻�
	///////////////////////////////////////////////////////////////////////////////////////
	if (!InterlockedCompareExchange128(
		(LONG64 *)pSession->_IOBlock,
		(LONG64)true,
		(LONG64)0,
		(LONG64 *)&stCompareBlock
		))
		return;

	pSession->_SessionInfo._Socket = INVALID_SOCKET;

	pSession->_SessionInfo._iPort = 0;
	memset(&pSession->_SessionInfo._wIP, 0, sizeof(pSession->_SessionInfo._wIP));

	pSession->_RecvQ.ClearBuffer();
	pSession->_SendQ.ClearBuffer();

	memset(&pSession->_RecvOverlapped, 0, sizeof(OVERLAPPED));
	memset(&pSession->_SendOverlapped, 0, sizeof(OVERLAPPED));

	for (int iSentCnt = 0; iSentCnt < pSession->_lSentPacketCnt; iSentCnt++)
	{
		CNPacket *pPacket = (CNPacket *)pSession->_pSentPacket[iSentCnt];
		pPacket->Free();
		pSession->_pSentPacket[iSentCnt] = nullptr;
	}

	pSession->_lSentPacketCnt = 0;

	InterlockedExchange((long *)&pSession->_bSendFlag, false);

	OnClientLeave(pSession->_iSessionID);

	pSession->_IOBlock->_iReleaseFlag = false;
	InsertBlankSessionIndex(GET_SESSIONINDEX(pSession->_iSessionID));

	InterlockedDecrement((LONG *)&_lSessionCount);
}



///////////////////////////////////////////////////////////////////////////////////////////
// ���� ������ �κ�
///////////////////////////////////////////////////////////////////////////////////////////
unsigned __stdcall	CNetServer::AcceptThread(LPVOID AcceptParam)
{
	return ((CNetServer *)AcceptParam)->AccpetThread_update();
}

unsigned __stdcall	CNetServer::WorkerThread(LPVOID WorkerParam)
{
	return ((CNetServer *)WorkerParam)->WorkerThread_update();
}

unsigned __stdcall	CNetServer::MonitorThread(LPVOID MonitorParam)
{
	return ((CNetServer *)MonitorParam)->MonitorThread_update();
}