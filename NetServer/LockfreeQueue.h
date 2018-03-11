#ifndef __LOCKFREEQUEUE__H__
#define __LOCKFREEQUEUE__H__


/*--------------------------------------------------------------------------*/
// Lockfree Queue
/*--------------------------------------------------------------------------*/
template <class DATA>
class CLockfreeQueue
{
private :
	/*----------------------------------------------------------------------*/
	// Lockfree Queue�� ��� ����ü
	/*----------------------------------------------------------------------*/
	struct st_NODE
	{
		DATA		Data;
		st_NODE		*pNext;
	};

	/*----------------------------------------------------------------------*/
	// Lockfree Queue�� Head, Tail ��� ����ü
	/*----------------------------------------------------------------------*/
	struct st_END_NODE
	{
		st_NODE		*pEndNode;
		__int64		iUniqueNum;
	};




public :
	//////////////////////////////////////////////////////////////////////////
	// ������
	//////////////////////////////////////////////////////////////////////////
				CLockfreeQueue()
	{
		_lUseSize			= 0;

		_iUniqueNumHead		= 0;
		_iUniqueNumTail		= 0;

		_pQueuePool			= new CMemoryPool<st_NODE>;

		//////////////////////////////////////////////////////////////////////
		// ���� ���
		//////////////////////////////////////////////////////////////////////
		st_NODE *pDummyNode	= _pQueuePool->Alloc();
		pDummyNode->pNext	= nullptr;

		_pHead				= (st_END_NODE *)_aligned_malloc(sizeof(st_END_NODE), 16);
		_pHead->pEndNode	= pDummyNode;
		_pHead->iUniqueNum	= 0;

		_pTail				= (st_END_NODE *)_aligned_malloc(sizeof(st_END_NODE), 16);
		_pTail->pEndNode	= pDummyNode;
		_pTail->iUniqueNum	= 0;
	}


	//////////////////////////////////////////////////////////////////////////
	// �Ҹ���
	//////////////////////////////////////////////////////////////////////////
	virtual		~CLockfreeQueue()
	{
		st_NODE *pDeleteNode;

		while (_pHead->pEndNode != nullptr)
		{
			pDeleteNode			= _pHead->pEndNode;
			_pHead->pEndNode	= _pHead->pEndNode->pNext;
			_pQueuePool->Free(pDeleteNode);
		}

		delete _pQueuePool;

		_aligned_free(_pHead);
		_aligned_free(_pTail);
	}


	//////////////////////////////////////////////////////////////////////////
	// Queue ����ֱ�
	//////////////////////////////////////////////////////////////////////////
	void		ClearBuffer(void)
	{
		st_NODE *pDeleteNode;

		while (!isEmpty())
		{
			pDeleteNode = _pHead->pEndNode;
			_pHead->pEndNode = _pHead->pEndNode->pNext;
			_pQueuePool->Free(pDeleteNode);

			_lUseSize--;
		}
	}


	//////////////////////////////////////////////////////////////////////////
	// Queue ������ ����
	//////////////////////////////////////////////////////////////////////////
	bool		Put(DATA Data)
	{
		st_END_NODE		stCloneTailNode;
		st_NODE			*pNextTail;

		__int64			iUniqueNumTail = InterlockedIncrement64((LONG64 *)&_iUniqueNumTail);

		//////////////////////////////////////////////////////////////////////
		// ������ ��� ����
		//////////////////////////////////////////////////////////////////////
		st_NODE			*pNode = _pQueuePool->Alloc();
		pNode->Data = Data;
		pNode->pNext = nullptr;

		while (1)
		{
			//////////////////////////////////////////////////////////////////
			// Tail ����
			//////////////////////////////////////////////////////////////////
			stCloneTailNode.iUniqueNum = _pTail->iUniqueNum;
			stCloneTailNode.pEndNode = _pTail->pEndNode;

			//////////////////////////////////////////////////////////////////
			// Tail�� Next ����
			//////////////////////////////////////////////////////////////////
			pNextTail = stCloneTailNode.pEndNode->pNext;

			//////////////////////////////////////////////////////////////////
			// Tail ������ NULL�̸�(Tail �������� ���ؼ�)
			// Node�� ���̰�
			// Tail�� �о��ش� 
			//
			// Tail�� �и��� �ʾƵ� �ٸ� ��(�ٸ� �������� Put, Get���� �о��ش�)
			//////////////////////////////////////////////////////////////////
			if (nullptr == pNextTail)
			{
				if (nullptr == InterlockedCompareExchangePointer(
					(PVOID *)&stCloneTailNode.pEndNode->pNext,
					pNode,
					pNextTail
					))
				{
					InterlockedCompareExchange128(
						(LONG64 *)_pTail,
						iUniqueNumTail,
						(LONG64)stCloneTailNode.pEndNode->pNext,
						(LONG64 *)&stCloneTailNode
						);
					break;
				}
			}
		

			//////////////////////////////////////////////////////////////////
			// next�� ������ �ϴ� �о���
			// 
			// Tail�� �ٲ���� ������ UniqueNum�� �÷���
			//////////////////////////////////////////////////////////////////
			else
			{
				InterlockedCompareExchange128(
					(LONG64 *)_pTail,
					iUniqueNumTail,
					(LONG64)stCloneTailNode.pEndNode->pNext,
					(LONG64 *)&stCloneTailNode
					);

				iUniqueNumTail = InterlockedIncrement64((LONG64 *)&_iUniqueNumTail);
			}
		}

		InterlockedIncrement((long *)&_lUseSize);

		return true;
	}



	//////////////////////////////////////////////////////////////////////////
	// Queue ������ ����
	//////////////////////////////////////////////////////////////////////////
	bool		Get(DATA *pOutData)
	{
		st_END_NODE		stCloneHeadNode, stCloneTailNode;
		st_NODE			*pNextHead;

		__int64			iUniqueNumHead = InterlockedIncrement64((LONG64 *)&_iUniqueNumHead);

		InterlockedDecrement((long *)&_lUseSize);	
		
		while (1)
		{
			//////////////////////////////////////////////////////////////////
			// Head ����
			//////////////////////////////////////////////////////////////////
			stCloneHeadNode.iUniqueNum = _pHead->iUniqueNum;
			stCloneHeadNode.pEndNode = _pHead->pEndNode;

			//////////////////////////////////////////////////////////////////
			// Tail ����
			//////////////////////////////////////////////////////////////////
			stCloneTailNode.iUniqueNum = _pTail->iUniqueNum;
			stCloneTailNode.pEndNode = _pTail->pEndNode;

			//////////////////////////////////////////////////////////////////
			// Head�� Next ����
			//////////////////////////////////////////////////////////////////
			pNextHead = stCloneHeadNode.pEndNode->pNext;

			//////////////////////////////////////////////////////////////////
			// ������� Ȯ��
			//////////////////////////////////////////////////////////////////
			if (isEmpty())
			{
				pOutData = nullptr;
				return false;
			}

			//////////////////////////////////////////////////////////////////
			// tail�ڿ� Node�� ������ �о��ֱ�
			//////////////////////////////////////////////////////////////////
			else if (nullptr != (stCloneTailNode.pEndNode->pNext))
			{
				__int64 iUniqueNumTail = InterlockedIncrement64((LONG64 *)&_iUniqueNumTail);
				InterlockedCompareExchange128(
					(LONG64 *)_pTail,
					iUniqueNumTail,
					(LONG64)stCloneTailNode.pEndNode->pNext,
					(LONG64 *)&stCloneTailNode
					);
			}

			//////////////////////////////////////////////////////////////////
			// Node�� ���� ���
			// Head���� ���Ͽ� �ٲ���
			//
			// ��尡 ���ٰ� �����µ� 
			// �� �������� ���� ��찡 ������ üũ�� ��
			// (Node�� ������ ����ִٰ� ������ ����)
			//  ---      ---
			// |   | -> |   |
			//  ---      ---
			//  ^ ^
			//  H T
			//////////////////////////////////////////////////////////////////
			else
			{
				if (nullptr != pNextHead)
				{
					*pOutData = pNextHead->Data;
					if (InterlockedCompareExchange128(
						(LONG64 *)_pHead,
						iUniqueNumHead,
						(LONG64)stCloneHeadNode.pEndNode->pNext,
						(LONG64 *)&stCloneHeadNode
						))
					{
						_pQueuePool->Free(stCloneHeadNode.pEndNode);
						break;
					}
				}
			}
		}
		return true;
	}


	//////////////////////////////////////////////////////////////////////////
	// Queue�� ������� Ȯ��
	//////////////////////////////////////////////////////////////////////////
	bool		isEmpty(void)
	{
		if (0 == _lUseSize)
		{
			if ((_pHead->pEndNode) == (_pTail->pEndNode))
				return true;
		}

		return false;
	}


	//////////////////////////////////////////////////////////////////////////
	// Queue ��뷮
	//////////////////////////////////////////////////////////////////////////
	long		GetUseSize(void) {	return _lUseSize;		}


	//////////////////////////////////////////////////////////////////////////
	// Queue�� �޸�Ǯ ��뷮
	//////////////////////////////////////////////////////////////////////////
	long		GetAllocSize(void) { return _pQueuePool->GetAllocCount(); }
	




private :
	//////////////////////////////////////////////////////////////////////////
	// Queue�� �޸� Ǯ
	//////////////////////////////////////////////////////////////////////////
	CMemoryPool<st_NODE>	*_pQueuePool;


	//////////////////////////////////////////////////////////////////////////
	// Queue�� Head, Tail
	//////////////////////////////////////////////////////////////////////////
	st_END_NODE				*_pHead;
	st_END_NODE				*_pTail;


	//////////////////////////////////////////////////////////////////////////
	// Head, Tail�� ����ũ �ѹ�
	//////////////////////////////////////////////////////////////////////////
	__int64					_iUniqueNumHead;
	__int64					_iUniqueNumTail;


	//////////////////////////////////////////////////////////////////////////
	// Queue ��뷮
	//////////////////////////////////////////////////////////////////////////
	long					_lUseSize;
};

#endif