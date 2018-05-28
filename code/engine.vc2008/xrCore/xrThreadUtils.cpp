//Giperion May 2018
//[EUREKA] 3.6
//Oxygen Team, Render Thread branch
#include "stdafx.h"
#include "xrThreadUtils.h"

xrCriticalSection::xrCriticalSection()
{
    InitializeCriticalSection(&guard);
}

xrCriticalSection::~xrCriticalSection()
{
    DeleteCriticalSection(&guard);
}

void xrCriticalSection::Enter()
{
    EnterCriticalSection(&guard);
}

void xrCriticalSection::Leave()
{
    LeaveCriticalSection(&guard);
}

xrCriticalSectionGuard::xrCriticalSectionGuard(xrCriticalSection* pSection)
    : pGuard(pSection)
{
    pGuard->Enter();
}

xrCriticalSectionGuard::~xrCriticalSectionGuard()
{
    pGuard->Leave();
    pGuard = nullptr;
}
