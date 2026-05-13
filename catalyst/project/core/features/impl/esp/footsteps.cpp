#include <stdafx.hpp>

namespace features::esp {

    void footsteps::tick( )
    {
        const auto& cfg = settings::g_esp.m_player.m_footsteps;
        if ( !cfg.enabled )
        {
            this->m_rings.clear( );
            return;
        }
        const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
        if ( !global_vars ) return;
        const float current_time = g::memory.read<float>( global_vars + 0x30 );
        const auto local_pawn = systems::g_local.pawn( );

        for ( const auto& player : systems::g_collector.players( ) )
        {
            if ( !player.pawn || !player.alive ) continue;
            if ( !systems::g_local.is_enemy( player.team ) && !cfg.show_teammates ) continue;
            if ( player.pawn == local_pawn ) continue;

            const std::uint32_t flags = g::memory.read<std::uint32_t>( player.pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) );
            const bool is_on_ground = ( flags & ( 1 << 0 ) ) != 0;
            bool was_on_ground = true;
            auto it = m_prev_on_ground.find( player.pawn );
            if ( it != m_prev_on_ground.end( ) ) was_on_ground = it->second;
            const float speed = player.velocity.length( );

            if ( !was_on_ground && is_on_ground && speed > 80.0f )
            {
                float rad = cfg.land_max_radius * std::min( 1.0f, speed / 400.0f );
                m_rings.push_back( { player.origin, std::max( cfg.land_max_radius * 0.5f, rad ), cfg.land_color, current_time } );
            }
            else if ( was_on_ground && !is_on_ground && speed > 50.0f )
            {
                m_rings.push_back( { player.origin, cfg.jump_max_radius, cfg.jump_color, current_time } );
            }
            else if ( was_on_ground && is_on_ground && speed > 50.0f )
            {
                float last_time = 0;
                auto lt = m_last_step_time.find( player.pawn );
                if ( lt != m_last_step_time.end( ) ) last_time = lt->second;
                if ( current_time - last_time > 0.25f )
                {
                    m_last_step_time[ player.pawn ] = current_time;
                    float rad = cfg.footstep_max_radius * std::min( 1.0f, speed / 250.0f );
                    m_rings.push_back( { player.origin, std::max( cfg.footstep_max_radius * 0.4f, rad ), cfg.footstep_color, current_time } );
                }
            }
            m_prev_on_ground[ player.pawn ] = is_on_ground;
        }

        const float max_age = cfg.expand_duration + cfg.fade_duration;
        m_rings.erase( std::remove_if( m_rings.begin( ), m_rings.end( ),
            [ current_time, max_age ]( const ring& r ) { return ( current_time - r.start_time ) > max_age; } ), m_rings.end( ) );
    }

    void footsteps::on_render( zdraw::draw_list& draw_list )
    {
        const auto& cfg = settings::g_esp.m_player.m_footsteps;
        if ( !cfg.enabled ) return;
        const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
        if ( !global_vars ) return;
        const float current_time = g::memory.read<float>( global_vars + 0x30 );

        for ( const auto& r : m_rings )
        {
            float elapsed = current_time - r.start_time;
            if ( elapsed < 0.0f || elapsed > ( cfg.expand_duration + cfg.fade_duration ) ) continue;
            float cur_rad, alpha;
            if ( elapsed < cfg.expand_duration )
            {
                float t = elapsed / cfg.expand_duration;
                cur_rad = r.max_radius * t;
                alpha = 1.0f;
            }
            else
            {
                cur_rad = r.max_radius;
                float fade_t = ( elapsed - cfg.expand_duration ) / cfg.fade_duration;
                alpha = 1.0f - fade_t;
            }
            if ( alpha < 0.01f ) continue;
            const auto screen = systems::g_view.project( r.world_pos );
            if ( !systems::g_view.projection_valid( screen ) ) continue;
            const auto edge = systems::g_view.project( r.world_pos + math::vector3{ cur_rad, 0, 0 } );
            if ( !systems::g_view.projection_valid( edge ) ) continue;
            float screen_rad = std::abs( edge.x - screen.x );
            if ( screen_rad < 1.0f ) screen_rad = 1.0f;
            zdraw::rgba col = r.color;
            col.a = static_cast< std::uint8_t >( col.a * alpha );
            draw_list.add_circle( screen.x, screen.y, screen_rad, col, cfg.segments, cfg.thickness );
        }
    }

}
