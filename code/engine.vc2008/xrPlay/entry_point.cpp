#include <string>
#include <windows.h>

#pragma comment(lib, "xrEngine.lib")
#pragma comment(lib, "xrLauncher.lib")

#define DLL_API __declspec(dllimport)
extern DLL_API int RunXRLauncher();
extern DLL_API const char* GetParams();
extern DLL_API int RunApplication(char* commandLine);

int APIENTRY WinMain(HINSTANCE hInsttance, HINSTANCE hPrevInstance, char* lpCmdLine, int nCmdShow)
{
    std::string params = lpCmdLine;
    if (strstr(lpCmdLine, "-launcher"))
    {
        const int l_res = RunXRLauncher();
        switch (l_res)
        {
        case 0: return 0;
        }
        params = GetParams(); // Fixed
    }

    RunApplication(params.data());

    return 0;
}