#pragma once
namespace Reun {
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using usize = size_t;

using f32 = float;
using f64 = double;

namespace DirX {
using typename DirectX::XMINT2;
using typename DirectX::XMINT3;
using typename DirectX::XMINT4;
using typename DirectX::XMMATRIX;

using typename DirectX::XMFLOAT3X3;
using typename DirectX::XMFLOAT4X4;
using typename DirectX::XMVECTOR;

using typename DirectX::XMUINT2;
using typename DirectX::XMUINT3;
using typename DirectX::XMUINT4;

using typename DirectX::XMFLOAT2;
using typename DirectX::XMFLOAT3;
using typename DirectX::XMFLOAT4;

using typename DirectX::PackedVector::XMBYTEN4;
using typename DirectX::PackedVector::XMUSHORTN2;

} // namespace DirX

using namespace DirX;

namespace Win {
using namespace winrt::Windows::Foundation::Numerics;
}
using namespace Win;
}; // namespace Reun
