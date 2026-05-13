#include <stdafx.hpp>

namespace {
    inline std::uint32_t compact1( std::uint32_t v )
    {
        v &= 0x09249249U;
        v = ( v ^ ( v >> 2 ) ) & 0x030C30C3U;
        v = ( v ^ ( v >> 4 ) ) & 0x0300F00FU;
        v = ( v ^ ( v >> 8 ) ) & 0x00000FFFU;
        return v;
    }
    inline void morton3_decode( std::uint32_t m, int& x, int& y, int& z )
    {
        x = (int)compact1( m );
        y = (int)compact1( m >> 1 );
        z = (int)compact1( m >> 2 );
    }
}

namespace systems {

    void voxels::update( )
    {
        m_active_voxels.clear( );
        auto gv = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
        if ( !gv ) return;
        int tick = g::memory.read<std::int32_t>( gv + 0x44 );
        for ( auto& proj : g_collector.projectiles( ) )
        {
            if ( proj.subtype != collector::projectile_subtype::smoke_grenade || !proj.smoke_active ) continue;
            int bt = g::memory.read<std::int32_t>( proj.entity + SCHEMA( "C_SmokeGrenadeProjectile", "m_nSmokeEffectTickBegin"_hash ) );
            if ( std::abs( tick - bt ) > 1400 ) continue;

            constexpr std::uint64_t emb = 0x14A8, st_off = 0x70, rgba_off = 0xE0, center_off = 0xE8, frame_off = 0x100, bitset_base = 4104;
            auto wrap = proj.entity + emb;
            auto state = g::memory.read<std::uint64_t>( wrap + st_off );
            auto rgba = g::memory.read<std::uint64_t>( wrap + rgba_off );
            auto center = g::memory.read<math::vector3>( wrap + center_off );
            auto frame = g::memory.read<std::int32_t>( wrap + frame_off );
            if ( !state || !rgba || frame < 0 || frame > 1024 ) continue;
            if ( center.x == 0 && center.y == 0 && center.z == 0 ) continue;

            std::array<std::uint64_t, 512> bitset{};
            if ( !g::memory.read( state + bitset_base + frame * 4096ULL, bitset.data( ), sizeof( bitset ) ) ) continue;

            auto vol = std::make_unique<std::uint8_t[ ]>( 131072 );
            if ( !g::memory.read( rgba, vol.get( ), 131072 ) ) continue;

            constexpr int n = 32;
            constexpr float scale = 20.0f, half = 16.0f;

            for ( std::uint32_t w = 0; w < 512; ++w )
            {
                std::uint64_t bits = bitset[ w ];
                while ( bits )
                {
                    std::uint32_t bit = std::countr_zero( bits );
                    bits &= bits - 1;
                    std::uint32_t mort = ( w << 6 ) | bit;
                    int x, y, z;
                    morton3_decode( mort, x, y, z );
                    int lin = x + n * ( y + n * z );
                    size_t idx = lin * 4;
                    math::vector3 pos = {
                        center.x + ( x - half ) * scale,
                        center.y + ( y - half ) * scale,
                        center.z + ( z - half ) * scale
                    };
                    m_active_voxels.push_back( { x, y, z, vol[ idx ], vol[ idx + 1 ], vol[ idx + 2 ], vol[ idx + 3 ], pos } );
                }
            }
        }
    }

    bool voxels::line_goes_through_smoke( const math::vector3& start, const math::vector3& end ) const
    {
        if ( m_active_voxels.empty( ) ) return false;
        const float half_sz = 11.0f;
        for ( auto& v : m_active_voxels )
        {
            math::vector3 bmin = { v.world.x - half_sz, v.world.y - half_sz, v.world.z - half_sz };
            math::vector3 bmax = { v.world.x + half_sz, v.world.y + half_sz, v.world.z + half_sz };
            if ( g_bvh.intersect_segment_aabb( start, end, bmin, bmax ) ) return true;
        }
        return false;
    }

}
