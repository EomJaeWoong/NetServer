#ifndef __LOCKFREESTACK__H__
#define __LOCKFREESTACK__H__


/*--------------------------------------------------------------------------*/
// Lockfree Stack
/*--------------------------------------------------------------------------*/
template <class DATA>
class CLockfreeStack
{
private :
	/*----------------------------------------------------------------------*/
	// Lockfree Stack�� ��� ����ü
	/*----------------------------------------------------------------------*/
	struct st_NODE
	{
		DATA		Data;
		st_NODE		*pNext;
	};

	/*----------------------------------------------------------------------*/
	// Lockfree Stack�� ž ��� ����ü
	/*----------------------------------------------------------------------*/
	struct st_TOP_NODE
	{
		st_NODE		*pTopNode;
		__int64		iUniqueNum;
	};



public :
	//////////////////////////////////////////////////////////////////////////
	// ������
	//////////////////////////////////////////////////////////////////////////
				CLockfreeStack()
	{
		_lUseSize		= 0;
		_iUniqueNum		= 0;

		_pStackPool		= new CMemoryPool<st_NODE>;

		_pTop			= (st_TOP_NODE *)_aligned_malloc(sizeof(st_TOP_NODE), 16);

		_pTop->pTopNode = nullptr;
		_pTop->iUniqueNum = 0;
	}


	//////////////////////////////////////////////////////////////////////////
	// �Ҹ���
	//////////////////////////////////////////////////////////////////////////
	virtual		~CLockfreeStack()
	{
		st_NODE *pDeleteNode = nullptr;

		for (int iCnt = 0; iCnt < _lUseSize; iCnt++)
		{
			pDeleteNode			= _pTop->pTopNode;
			_pTop->pTopNode		= _pTop->pTopNode->pNext;
			_pStackPool->Free(pDeleteNode);
		}

		delete _pStackPool;
		_aligned_free(_pTop);
	}


	//////////////////////////////////////////////////////////////////////////
	// Stack ������ ����
	//////////////////////////////////////////////////////////////////////////
	bool		Push(DATA Data)
	{
		st_TOP_NODE stCloneTopNode;
		st_NODE		*pNode		= _pStackPool->Alloc();
		if (nullptr == pNode)
			return false;

		pNode->Data = Data;

		__int64		iUniqueNum  = InterlockedIncrement64((LONG64 *)&_iUniqueNum);

		do
		{
			stCloneTopNode.iUniqueNum	= _pTop->iUniqueNum;
			stCloneTopNode.pTopNode		= _pTop->pTopNode;

			pNode->pNext				= _pTop->pTopNode;
		} while (!InterlockedCompareExchange128(
			(LONG64 *)_pTop,
			iUniqueNum,
			(LONG64)pNode,
			(LONG64 *)&stCloneTopNode
			));

		InterlockedIncrement((long *)&_lUseSize);

		return true;
	}


	//////////////////////////////////////////////////////////////////////////
	// stack���� ������ ����
	//////////////////////////////////////////////////////////////////////////
	bool		Pop(DATA *pOutData)
	{
		st_TOP_NODE stCloneTopNode;

		//////////////////////////////////////////////////////////////////////
		// �����Ͱ� ������
		//////////////////////////////////////////////////////////////////////
		if (isEmpty())
		{
			pOutData = nullptr;
			return false;
		}

		InterlockedDecrement((long *)&_lUseSize);

		__int64		iUniqueNum = InterlockedIncrement64((LONG64 *)&_iUniqueNum);

		do
		{
			stCloneTopNode.iUniqueNum	= _pTop->iUniqueNum;
			stCloneTopNode.pTopNode		= _pTop->pTopNode;
		} while (!InterlockedCompareExchange128(
			(LONG64 *)_pTop,
			iUniqueNum,
			(LONG64)_pTop->pTopNode->pNext,
			(LONG64 *)&stCloneTopNode
			));

		*pOutData = stCloneTopNode.pTopNode->Data;

		_pStackPool->Free(stCloneTopNode.pTopNode);

		return true;
	}


	//////////////////////////////////////////////////////////////////////////
	// ������ ������� Ȯ��
	//////////////////////////////////////////////////////////////////////////
	bool		isEmpty(void)
	{
		if (0 == _lUseSize)
		{
			if (nullptr == _pTop->pTopNode)
				return true;
		}

		return false;
	}


	//////////////////////////////////////////////////////////////////////////
	// stack ��뷮
	//////////////////////////////////////////////////////////////////////////
	long		GetUseSize(void) {	return _lUseSize;	 }


	//////////////////////////////////////////////////////////////////////////
	// ������ �޸�Ǯ �Ҵ� ����
	//////////////////////////////////////////////////////////////////////////
	long		GetAllocSize(void){	return _pStackPool->GetAllocCount();	}



private :
	//////////////////////////////////////////////////////////////////////////
	// Stack�� �޸� Ǯ
	//////////////////////////////////////////////////////////////////////////
	CMemoryPool<st_NODE>		*_pStackPool;


	//////////////////////////////////////////////////////////////////////////
	// ž ���
	//////////////////////////////////////////////////////////////////////////
	st_TOP_NODE					*_pTop;


	//////////////////////////////////////////////////////////////////////////
	// ž ����� ����ũ �ѹ�
	//////////////////////////////////////////////////////////////////////////
	__int64						_iUniqueNum;


	//////////////////////////////////////////////////////////////////////////
	// Stack ��뷮
	//////////////////////////////////////////////////////////////////////////
	long						_lUseSize;
};

#endif