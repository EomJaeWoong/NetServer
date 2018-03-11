/////////////////////////////////////////////////////////////////////
// www.gamecodi.com						������ master@gamecodi.com
//
//
/////////////////////////////////////////////////////////////////////
/*---------------------------------------------------------------

	Aya Library - Stream SingleQueue

	���� FIFO ��Ʈ���� ť.


	�ϼ��� ��Ŷ�� ����� ���ؼ� StreamDQ �մܿ� ����� �������� �Ѵ�.


	- ����.

	CAyaStreamSQ	clStreamQ;
	CAyaStreamSQ	clStreamQ(1024);




----------------------------------------------------------------*/
#ifndef __AYA_STREAM_SINGLE_QUEUE__
#define __AYA_STREAM_SINGLE_QUEUE__

class CAyaStreamSQ
{
public:

	/*---------------------------------------------------------------
	AyaStreamSQ Enum.

	----------------------------------------------------------------*/
	enum e_AYA_STREAM_SQ
	{
		eBUFFER_DEFAULT				= 20960,		// ������ �⺻ ũ��.
		eBUFFER_BLANK				= 8			 	// Ȯ���� ������ ���� 8Byte �� �����.

	};

public:

	/////////////////////////////////////////////////////////////////////////
	// ������(���� ũ�� ����), �ı���.
	//
	//
	/////////////////////////////////////////////////////////////////////////
	CAyaStreamSQ(void)
	{
		Initial(eBUFFER_DEFAULT);
	}


	CAyaStreamSQ(int iBufferSize)
	{
		Initial(iBufferSize);
	}

	virtual	~CAyaStreamSQ(void)
	{
		delete[] m_chpBuffer;

		DeleteCriticalSection(&m_csQueue);
	}

	/////////////////////////////////////////////////////////////////////////
	// ���� ���� & ť�� �ʱ�ȭ.
	//
	// Parameters: (int)���ۿ뷮.
	// Return: ����.
	/////////////////////////////////////////////////////////////////////////
	void	Initial(int iBufferSize)
	{
		m_iBufferSize = iBufferSize;

		m_chpBuffer = (char *)malloc(iBufferSize);

		m_iReadPos = 0;
		m_iWritePos = 0;

		InitializeCriticalSection(&m_csQueue);
	}

	/////////////////////////////////////////////////////////////////////////
	// ���� ��ü�� �뷮 ����.
	//
	// Parameters: ����.
	// Return: (int)���ۿ뷮.
	/////////////////////////////////////////////////////////////////////////
	int		GetBufferSize(void){	return m_iBufferSize;	}

	/////////////////////////////////////////////////////////////////////////
	// ���� ������� �뷮 ���.
	//
	// Parameters: ����.
	// Return: (int)������� �뷮.
	/////////////////////////////////////////////////////////////////////////
	int		GetUseSize(void)
	{
		if (m_iReadPos <= m_iWritePos)
			return m_iWritePos - m_iReadPos;

		else
			return (m_iBufferSize - m_iReadPos) + m_iWritePos;
	}

	/////////////////////////////////////////////////////////////////////////
	// ���� ���ۿ� ���� �뷮 ���.
	//
	// Parameters: ����.
	// Return: (int)�����뷮.
	/////////////////////////////////////////////////////////////////////////
	int		GetFreeSize(void)
	{
		if (m_iReadPos > m_iWritePos)
			return m_iReadPos - m_iWritePos - eBUFFER_BLANK;

		else
			return (m_iBufferSize - m_iWritePos) + m_iReadPos - eBUFFER_BLANK;
	}

	/////////////////////////////////////////////////////////////////////////
	// ���� �����ͷ� �ܺο��� �ѹ濡 �а�, �� �� �ִ� ����.
	// (������ ���� ����)
	//
	// Parameters: ����.
	// Return: (int)��밡�� �뷮.
	/////////////////////////////////////////////////////////////////////////
	int		GetNotBrokenGetSize(void)
	{
		if (m_iReadPos <= m_iWritePos)
			return m_iWritePos - m_iReadPos;

		else
			return m_iBufferSize - m_iReadPos;
	}

	int		GetNotBrokenPutSize(void)
	{
		if (m_iReadPos > m_iWritePos)
			return m_iReadPos - m_iWritePos - eBUFFER_BLANK;

		else
		{
			if (m_iReadPos < eBUFFER_BLANK)
				return m_iBufferSize - m_iWritePos - (eBUFFER_BLANK - m_iReadPos);
			else
				return m_iBufferSize - m_iWritePos;
		}
			
	}


	/////////////////////////////////////////////////////////////////////////
	// WritePos �� ����Ÿ ����.
	//
	// Parameters: (char *)����Ÿ ������. (int)ũ��. 
	// Return: (int)���� ũ��.
	/////////////////////////////////////////////////////////////////////////
	int		Put(char *chpData, int iSize)
	{
		if (iSize > GetFreeSize())
			iSize = GetFreeSize();

		for (int iCnt = 0; iCnt < iSize; iCnt++)
		{
			m_chpBuffer[m_iWritePos] = chpData[iCnt];
			m_iWritePos = (m_iWritePos + 1) % m_iBufferSize;
		}

		return iSize;
	}

	/////////////////////////////////////////////////////////////////////////
	// ReadPos ���� ����Ÿ ������. ReadPos �̵�.
	//
	// Parameters: (char *)����Ÿ ������. (int)ũ��.
	// Return: (int)������ ũ��.
	/////////////////////////////////////////////////////////////////////////
	int		Get(char *chpDest, int iSize)
	{
		if (iSize > GetUseSize())
			iSize = GetUseSize();

		for (int iCnt = 0; iCnt < iSize; iCnt++)
		{
			chpDest[iCnt] = m_chpBuffer[m_iReadPos];
			m_iReadPos = (m_iReadPos + 1) % m_iBufferSize;
		}

		return iSize; 
	}

	/////////////////////////////////////////////////////////////////////////
	// ReadPos ���� ����Ÿ �о��. ReadPos ����.
	//
	// Parameters: (char *)����Ÿ ������. (int)ũ��.
	// Return: (int)������ ũ��.
	/////////////////////////////////////////////////////////////////////////
	int		Peek(char *chpDest, int iSize)
	{
		if (iSize > GetUseSize())
			iSize = GetUseSize();

		if (iSize > GetNotBrokenGetSize())
			iSize = GetNotBrokenGetSize();

		for (int iCnt = 0; iCnt < iSize; iCnt++)
		{
			chpDest[iCnt] = m_chpBuffer[(m_iReadPos + iCnt) % m_iBufferSize];
		}

		return iSize;
	}





	/////////////////////////////////////////////////////////////////////////
	// ���ϴ� ���̸�ŭ �б���ġ ���� ����.
	//
	// Parameters: ����.
	// Return: ����.
	/////////////////////////////////////////////////////////////////////////
	void		RemoveData(int iSize)
	{
		if (GetUseSize() < iSize)
			iSize = GetUseSize();
		
		m_iReadPos = (m_iReadPos + iSize) % m_iBufferSize;
	}

	/////////////////////////////////////////////////////////////////////////
	// Write �� ��ġ�� �̵��ϴ� �Լ�.
	//
	// Parameters: ����.
	// Return: (int)Write �̵� ������
	/////////////////////////////////////////////////////////////////////////
	int	MoveWritePos(int iSize)
	{
		if (GetFreeSize() < iSize)
			iSize = GetFreeSize();
		
		m_iWritePos = (m_iWritePos + iSize) % m_iBufferSize;

		return iSize;
	}


	/////////////////////////////////////////////////////////////////////////
	// ������ ��� ����Ÿ ����.
	//
	// Parameters: ����.
	// Return: ����.
	/////////////////////////////////////////////////////////////////////////
	void	ClearBuffer(void)
	{
		m_iReadPos = 0;
		m_iWritePos = 0;
	}

	/////////////////////////////////////////////////////////////////////////
	// ������ ������ ����.
	//
	// Parameters: ����.
	// Return: (char *) ���� ������.
	/////////////////////////////////////////////////////////////////////////
	char	*GetBufferPtr(void){	return m_chpBuffer;		}

	/////////////////////////////////////////////////////////////////////////
	// ������ ReadPos ������ ����.
	//
	// Parameters: ����.
	// Return: (char *) ���� ������.
	/////////////////////////////////////////////////////////////////////////
	char	*GetReadBufferPtr(void){	return &m_chpBuffer[m_iReadPos];	 }

	/////////////////////////////////////////////////////////////////////////
	// ������ WritePos ������ ����.
	//
	// Parameters: ����.
	// Return: (char *) ���� ������.
	/////////////////////////////////////////////////////////////////////////
	char	*GetWriteBufferPtr(void){ return &m_chpBuffer[m_iWritePos];		 }



	/////////////////////////////////////////////////////////////////////////
	// Lock, Unlock.
	// ������ ȯ���� ���� CriticalSection ���� Lock, Unlock ó��.
	// 
	// Parameters: ����.
	// Return: ����.
	/////////////////////////////////////////////////////////////////////////
	void	Lock(void){		EnterCriticalSection(&m_csQueue);	}
	void	Unlock(void){	LeaveCriticalSection(&m_csQueue);	}





protected:

	//------------------------------------------------------------
	// ���� ������.
	//------------------------------------------------------------
	char				*m_chpBuffer;
	//------------------------------------------------------------
	// ���� ������.
	//------------------------------------------------------------
	int					m_iBufferSize;

	public :

	//------------------------------------------------------------
	// ������ �б� ��ġ, ���� ��ġ.
	//------------------------------------------------------------
	int					m_iReadPos;
	int					m_iWritePos;

	//------------------------------------------------------------
	// ������ ȯ���� ���� ũ��Ƽ�� ���� ���.
	//------------------------------------------------------------
	CRITICAL_SECTION	m_csQueue;


};


#endif