long ThreadInterlockedExchange( long volatile * pDest, long value )                           { Assert( (size_t)pDest % 4 == 0 ); return __atomic_exchange_n( pDest, value, __ATOMIC_ACQ_REL ); }
bool ThreadInterlockedAssignIf( long volatile * pDest, long value, long comperand )           { Assert( (size_t)pDest % 4 == 0 ); return __atomic_compare_exchange_n( pDest, &comperand, value, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED ); }