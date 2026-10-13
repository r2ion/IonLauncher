#include "vstdlib/random.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#define RANDOM_MULTIPLIER 48271
#define RANDOM_MODULUS 2147483647
#define RANDOM_QUOTIENT 44488
#define RANDOM_REMAINDER 3399
#define MAX_RANDOM_RANGE 0x7FFFFFFFu
#define MAX_RANDOM_RANGE_SHORT 0x7FFF
#define RANDOM_SCALE 1.0f / RANDOM_MODULUS
#define MAX_RANDOM_FLOAT 1.0f - 1.2e-7f

CUniformRandomStream::CUniformRandomStream()
{
    SetSeed(0);
}

CUniformRandomStream::~CUniformRandomStream() = default;

void CUniformRandomStream::SetSeed(const int seed)
{
    AUTO_LOCK(m_mutex);
    m_idum = seed;
}

int CUniformRandomStream::GetSeed()
{
    return m_idum;
}

int CUniformRandomStream::GenerateRandomNumber()
{
    AUTO_LOCK(m_mutex);

    if (m_idum <= 0)
    {
        if (m_idum == std::numeric_limits<int>::min() || -m_idum < 1)
            m_idum = 1;
        else
            m_idum = -m_idum;
    }

    const int quotient = m_idum / RANDOM_QUOTIENT;
    m_idum = RANDOM_MULTIPLIER * (m_idum - quotient * RANDOM_QUOTIENT) - RANDOM_REMAINDER * quotient;
    if (m_idum < 0)
        m_idum += RANDOM_MODULUS;

    return m_idum;
}

float CUniformRandomStream::RandomFloat(const float minimum, const float maximum)
{
    const float unit = std::min(RANDOM_SCALE * GenerateRandomNumber(), MAX_RANDOM_FLOAT);
    return unit * (maximum - minimum) + minimum;
}

int CUniformRandomStream::RandomInt(const int minimum, const int maximum)
{
    Assert(minimum <= maximum);

    const std::uint32_t range = static_cast<std::uint32_t>(maximum) - static_cast<std::uint32_t>(minimum) + 1;
    if (range <= 1 || range - 1 > MAX_RANDOM_RANGE)
        return minimum;

    const std::uint32_t maximumAcceptable = MAX_RANDOM_RANGE - ((MAX_RANDOM_RANGE + 1) % range);
    std::uint32_t value;
    do
    {
        value = static_cast<std::uint32_t>(GenerateRandomNumber());
    } while (value > maximumAcceptable);

    return static_cast<int>(static_cast<std::uint32_t>(minimum) + value % range);
}

float CUniformRandomStream::RandomFloatExp(const float minimum, const float maximum, const float exponent)
{
    float unit = std::min(RANDOM_SCALE * GenerateRandomNumber(), MAX_RANDOM_FLOAT);
    if (exponent != 1.0f)
        unit = std::pow(unit, exponent);
    return unit * (maximum - minimum) + minimum;
}

int CUniformRandomStream::RandomIntZeroMax()
{
    return RandomInt(0, MAX_RANDOM_RANGE_SHORT);
}
