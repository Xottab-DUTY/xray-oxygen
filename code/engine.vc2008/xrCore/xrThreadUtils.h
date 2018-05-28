//Giperion May 2018
//[EUREKA] 3.6
//Oxygen Team, Render Thread branch
#pragma once

class XRCORE_API xrCriticalSection
{
    CRITICAL_SECTION guard;

public:
    xrCriticalSection();
    ~xrCriticalSection();

    void Enter();
    void Leave();
};


class XRCORE_API xrCriticalSectionGuard
{
    xrCriticalSection* pGuard = nullptr;
public:
    xrCriticalSectionGuard(xrCriticalSection* pSection);
    ~xrCriticalSectionGuard();
};