//Giperion March 2018
//[EUREKA] 3.6
//X-Ray Oxygen 1.7 project
#pragma once


struct xrShaderInclude
{
    shared_str Filename;
    u32 References;
    u32 CRC32;

};

class xrShaderIncludeResolver
{
public:

    void ProcessShaderCode(LPCSTR shaderName, LPCSTR pShaderCode, u32 CodeSize);

    void FindOrAddShaderInclude(LPCSTR pShaderFileName);

private:

    //xr_vector<shared_str> 
    xr_map<shared_str, xrShaderInclude> m_includeMap;
};