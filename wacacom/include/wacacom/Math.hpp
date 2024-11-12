#pragma once

#include <type_traits>

template <class T>
constexpr T lmap(T x, T minA, T maxA, T minB, T maxB) requires (std::is_arithmetic_v<T>)
{
    return minB + ((x - minA) * (maxB - minB))/(maxA - minA);
}

template <class T>
constexpr T normalize(T x, T min, T max) requires (std::is_arithmetic_v<T>)
{
    return (x - min)/(max - min);
}

