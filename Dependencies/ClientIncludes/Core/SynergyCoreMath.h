// Common Math symbols declarations.

#include <cstdint>
#include <math.h> // Could we get rid of this at some point ?

// Core template for Vectors of 2 scalar elements.
// Defines operator overloads.
template<typename ScalarType>
struct Vector2
{
	ScalarType x, y;
};

// Construction

template<typename ScalarType>
constexpr Vector2<ScalarType> MakeVec2(ScalarType X, ScalarType Y)
{
	return { X, Y };
}

// Operator overloads

template<typename ScalarType, typename ScalarType2>
constexpr Vector2<ScalarType> operator+(const Vector2<ScalarType>& A, const Vector2<ScalarType2>& B)
{
	return { A.x + B.x, A.y + B.y };
}

template<typename ScalarType, typename ScalarType2>
constexpr Vector2<ScalarType> operator-(const Vector2<ScalarType>& A, const Vector2<ScalarType2>& B)
{
	return { A.x - B.x, A.y - B.y };
}

template<typename ScalarType, typename ScalarType2>
constexpr Vector2<ScalarType> operator*(const Vector2<ScalarType>& A, const ScalarType2& B)
{
	return { A.x * B, A.y * B };
}

template<typename ScalarType, typename ScalarType2>
constexpr Vector2<ScalarType> operator/(const Vector2<ScalarType>& A, const ScalarType2& B)
{
	return { A.x / B, A.y / B };
}

// Utility Functions
// Note: These are not really written for performance, just convenience. Any heavy operation with many vectors getting manipulated should
// feature their own performant solution such as using Simd or whatever else is possible in their own context.

template<typename ScalarType>
constexpr float VecLength(const Vector2<ScalarType>& Vec)
{
	return sqrtf(Vec.x * Vec.x + Vec.y * Vec.y);
}

template<typename ScalarType>
constexpr Vector2<ScalarType> VecNormalized(const Vector2<ScalarType>& Vec)
{
	return Vec / VecLength(Vec);
}

// Floating types
typedef Vector2<float> Vector2f;
typedef Vector2<double> Vector2d;

// Integer types
typedef Vector2<uint16_t> Vector2s;
typedef Vector2<uint32_t> Vector2i;
typedef Vector2<uint64_t> Vector2l;