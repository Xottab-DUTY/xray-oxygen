//Giperion March 2018
//[EUREKA] 3.6
//X-Ray Oxygen 1.7 project
#include "stdAfx.h"
#include "shaderIncludeResolver.h"


void xrShaderIncludeResolver::ProcessShaderCode(LPCSTR shaderName, LPCSTR pShaderCode, u32 CodeSize)
{
    if (CodeSize == 0)
    {
        Msg("! Empty shader %s", shaderName);
        return;
    }

    SStringVec Lines = xr_string::Split(pShaderCode, CodeSize, '\n');

    for (xr_string& Line : Lines)
    {
        xr_string ClearedLine = Line.RemoveWhitespaces();
        if (ClearedLine.StartWith("#include"))
        {

        }
    }
}

void xrShaderIncludeResolver::FindOrAddShaderInclude(LPCSTR pShaderFileName)
{

}
