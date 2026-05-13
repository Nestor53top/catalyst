#include <stdafx.hpp>

namespace features::esp {

    void footsteps::tick( )
    {
        auto& cfg = settings::g_esp.m_player.m_footsteps;
        if ( !cfg.enabled ) { m_rings.clear( ); return; }
        auto gv = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
        if ( !gv ) return;
        float now = g::memory.read<float>( gv + 0x30 );
        auto self = systems::g_local.pawn( );

        for ( auto& p : systems::g_collector.players( ) )
        {
            if ( !p.pawn || !p.alive ) continue;
            if ( !systems::g_local.is_enemy( p.team ) && !cfg.show_teammates ) continue;
            if ( p.pawn == self ) continue;

            std::uint32_t fl = g::memory.read<std::uint32_t>( p.pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) );
            bool on_ground = ( fl & 1 ) != 0;
            bool was = m_prev_on_ground[ p.pawn ];
            float spd = p.velocity.length( );

            if ( !was && on_ground && spd > 80.0f )
            {
                float rad = cfg.land_max_radius * std::min( 1.0f, spd / 400.0f );
                m_rings.push_back( { p.origin, std::max( cfg.land_max_radius * 0.5f, rad ), cfg.land_color, now } );
            }
            else if ( was && !on_ground && spd > 50.0f )
                m_rings.push_back( { p.origin, cfg.jump_max_radius, cfg.jump_color, now } );
            else if ( was && on_ground && spd > 50.0f )
            {
                float last = m_last_step_time[ p.pawn ];
                if ( now - last > 0.25f )
                {
                    m_last_step_time[ p.pawn ] = now;
                    float rad = cfg.footstep_max_radius * std::min( 1.0f, spd / 250.0f );
                    m_rings.push_back( { p.origin, std::max( cfg.footstep_max_radius * 0.4f, rad ), cfg.footstep_color, now } );
                }
            }
            m_prev_on_ground[ p.pawn ] = on_ground;
        }

        float max_age = cfg.expand_duration + cfg.fade_duration;
        m_rings.erase( std::remove_if( m_rings.begin( ), m_rings.end( ), [&]( auto& r ) { return now - r.start_time > max_age; } ), m_rings.end( ) );
    }

    void footsteps::on_render( zdraw::draw_list& dl )
    {
        auto& cfg = settings::g_esp.m_player.m_footsteps;
        if ( !cfg.enabled ) return;
        auto gv = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
        if ( !gv ) return;
        float now = g::memory.read<float>( gv + 0x30 );

        for ( auto& r : m_rings )
        {
            float el = now - r.start_time;
            if ( el < 0 || el > cfg.expand_duration + cfg.fade_duration ) continue;
            float rad, alpha;
            if ( el < cfg.expand_duration )
            {
                float t = el / cfg.expand_duration;
                rad = r.max_radius * t;
                alpha = 1.0f;
            }
            else
            {
                rad = r.max_radius;
                float t = ( el - cfg.expand_duration ) / cfg.fade_duration;
                alpha = 1.0f - t;
            }
            if ( alpha < 0.01f ) continue;
            auto scr = systems::g_view.project( r.world_pos );
            if ( !systems::g_view.projection_valid( scr ) ) continue;
            auto edge = systems::g_view.project( r.world_pos + math::vector3{ rad, 0, 0 } );
            if ( !systems::g_view.projection_valid( edge ) ) continue;
            float screen_rad = std::abs( edge.x - scr.x );
            if ( screen_rad < 1.0f ) screen_rad = 1.0f;
            auto col = r.color;
            col.a = (std::uint8_t)( col.a * alpha );
            dl.add_circle( scr.x, scr.y, screen_rad, col, cfg.segments, cfg.thickness );
        }
    }

}
