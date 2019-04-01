#pragma once
#include "../xrEngine/Environment.h"
#include "EnvDescriptorMixer.h"

namespace XRay
{
	public ref class MEnvironment
	{

	internal:
		CEnvironment* pNativeObject;

	public:
		EnvDescriptorMixer^	CurrentEnv;


	public:
		
		MEnvironment(::System::IntPtr InNativeLevel);
		MEnvironment(CEnvironment& environment());


		static void ChangeGameTime(float fValue);

		
	};

}




