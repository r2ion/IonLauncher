#pragma once

#include "tier0/threadtools.h"

#define VENGINE_RANDOM_INTERFACE_VERSION "VEngineRandom001"

class IUniformRandomStream
{
public:
	virtual void SetSeed(int seed) = 0; // 0
	virtual int GetSeed() = 0; // 1
	virtual float RandomFloat(float minimum = 0.0f, float maximum = 1.0f) = 0; // 2
	virtual int RandomInt(int minimum, int maximum) = 0; // 3
	virtual float RandomFloatExp(float minimum = 0.0f, float maximum = 1.0f, float exponent = 1.0f) = 0; // 4
	virtual int RandomIntZeroMax() = 0; // 5
};

static_assert(sizeof(IUniformRandomStream) == sizeof(void*));

class CUniformRandomStream final : public IUniformRandomStream
{
public:
	CUniformRandomStream();
	virtual ~CUniformRandomStream();

	void SetSeed(int seed) override;
	int GetSeed() override;
	float RandomFloat(float minimum = 0.0f, float maximum = 1.0f) override;
	int RandomInt(int minimum, int maximum) override;
	float RandomFloatExp(float minimum = 0.0f, float maximum = 1.0f, float exponent = 1.0f) override;
	int RandomIntZeroMax() override;

private:
	int GenerateRandomNumber();

	int m_idum;
	CThreadMutex m_mutex;
};

static_assert(sizeof(CUniformRandomStream) == 56);

extern "C"
{
	__declspec(dllimport) int GetCurrentRandomSeed();
	__declspec(dllimport) void InstallUniformRandomStream(IUniformRandomStream* stream);
	__declspec(dllimport) float RandomFloat(float minimum = 0.0f, float maximum = 1.0f);
	__declspec(dllimport) float RandomFloatExp(float minimum = 0.0f, float maximum = 1.0f, float exponent = 1.0f);
	__declspec(dllimport) float RandomGaussianFloat(float mean = 0.0f, float standardDeviation = 1.0f);
	__declspec(dllimport) int RandomInt(int minimum, int maximum);
	__declspec(dllimport) int RandomIntZeroMax();
	__declspec(dllimport) void RandomSeed(int seed);
}
