#ifndef RDSDK_ANTILAG_H
#define RDSDK_ANTILAG_H

void Radeon_EnableLowLatencySDK(bool enable);
bool Radeon_IsLowLatencySDKAvailable();

bool Radeon_InitLowLatencySDK();
void Radeon_ShutdownLowLatencySDK();

void Radeon_RunLowLatencyFrame(unsigned int maxFPS);

#endif // RDSDK_ANTILAG_H
