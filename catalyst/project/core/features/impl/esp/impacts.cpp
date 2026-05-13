#include <stdafx.hpp>

namespace features::esp {

    void impacts::on_render( zdraw::draw_list& dl )
    {
        auto& cfg = settings::g_esp.m_bullet_tracers;
        if ( !cfg.enabled || m_tracers.empty( ) ) return;
        auto& vm = systems::g_view.matrix( );
        auto get_w = [&]( const math::vector3& p ) {
            return vm[ 3 ][ 0 ] * p.x + vm[ 3 ][ 1 ] * p.y + vm[ 3 ][ 2 ] * p.z + vm[ 3 ][ 3 ];
        };
        for ( auto& t : m_tracers )
        {
            math::vector3 p1 = t.start, p2 = t.end;
            float w1 = get_w( p1 ), w2 = get_w( p2 );
            if ( w1 < 0.01f && w2 < 0.01f ) continue;
            if ( w1 < 0.01f ) { float f = ( 0.01f - w1 ) / ( w2 - w1 ); p1 = p1 + ( p2 - p1 ) * f; }
            if ( w2 < 0.01f ) { float f = ( 0.01f - w1 ) / ( w2 - w1 ); p2 = p1 + ( p2 - p1 ) * f; }
            auto s1 = systems::g_view.project( p1 ), s2 = systems::g_view.project( p2 );
            if ( !systems::g_view.projection_valid( s1 ) || !systems::g_view.projection_valid( s2 ) ) continue;
            float alpha = std::clamp( t.time / t.max_time, 0.0f, 1.0f );
            auto col = zdraw::rgba{ cfg.color.r, cfg.color.g, cfg.color.b, (std::uint8_t)( alpha * cfg.color.a ) };
            dl.add_line( s1.x, s1.y, s2.x, s2.y, col, cfg.thickness );
            if ( alpha > 0.2f )
                dl.add_line( s1.x, s1.y, s2.x, s2.y, zdraw::rgba{ 255,255,255,(std::uint8_t)( alpha * 80 ) }, cfg.thickness * 0.5f );
        }
    }

    void impacts::tick( )
    {
        auto& cfg = settings::g_esp.m_bullet_tracers;
        auto pawn = systems::g_local.pawn( );
        if ( !pawn ) { m_tracers.clear( ); m_old_shots = 0; return; }
        int shots = g::memory.read<int>( pawn + SCHEMA( "C_CSPlayerPawn", "m_iShotsFired"_hash ) );
        if ( shots > m_old_shots )
        {
            auto start = systems::g_view.origin( );
            math::vector3 fwd, right, up;
            systems::g_view.angles( ).to_directions( &fwd, &right, &up );
            auto muzzle = start + right * 4.0f + up * -3.0f + fwd * 10.0f;
            auto end = start + fwd * 8192.0f;
            auto trace = systems::g_bvh.trace_ray( start, end );
            auto hit = trace.hit ? trace.end_pos : end;
            m_tracers.push_back( { muzzle, hit, (float)cfg.duration, (float)cfg.duration } );
        }
        m_old_shots = shots;
        static auto last = std::chrono::steady_clock::now( );
        auto now = std::chrono::steady_clock::now( );
        float dt = std::chrono::duration<float, std::milli>( now - last ).count( );
        last = now;
        for ( auto it = m_tracers.begin( ); it != m_tracers.end( ); )
        {
            it->time -= dt;
            if ( it->time <= 0 ) it = m_tracers.erase( it );
            else ++it;
        }
    }

}
