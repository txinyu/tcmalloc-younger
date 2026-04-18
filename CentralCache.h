#pragma once 

#include "Common.h"

class CentralCache
{
public:
	static CentralCache* GetInstrance()
	{
		return &_sInt;
	}
	Span* GetOneSpan(SpanList& list, size_t byte_size);
	
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	void ReleaseListToSpans(void* start, size_t byte_size);
private:
	SpanList _spanLists[NFREELIST];
private:
	CentralCache()
	{ }
	CentralCache(const CentralCache&) = delete;
	static CentralCache _sInt;
};
