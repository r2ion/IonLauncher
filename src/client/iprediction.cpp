#include "client/iprediction.h"
#include "core/tier1.h"

IPrediction* g_pClientSidePrediction;

ON_DLL_LOAD_CLIENT("client.dll", ClientPrediction, [](CModule)
{
	g_pClientSidePrediction =
		Sys_GetFactoryPtr("client.dll", VCLIENT_PREDICTION_INTERFACE_VERSION).RCast<IPrediction*>();
})
