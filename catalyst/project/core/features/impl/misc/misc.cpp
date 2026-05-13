#include <stdafx.hpp>
#include "sounds.h"

namespace features::misc {

    void misc_features::on_render( zdraw::draw_list& dl )
    {
        std::unique_lock lock( m_mutex );
        draw_watermark( dl );
        draw_spectators( dl );
        draw_bomb_timer( dl );
        draw_hit_markers( dl );
        draw_damage_indicators( dl );
    }

    void misc_features::tick_write( ) { fov_changer( ); }

    void misc_features::tick( )
    {
        hitsounds( );
        auto now = std::chrono::steady_clock::now( );
        std::unique_lock lock( m_mutex );
        for ( auto& p : systems::g_collector.players( ) )
        {
            if ( !systems::g_local.is_enemy( p.team ) ) { m_health_history.erase( p.pawn ); continue; }
            auto& hist = m_health_history[ p.pawn ];
            if ( hist.health == -1 ) { hist.health = p.health; hist.origin = p.origin; continue; }
            if ( p.health < hist.health && settings::g_esp.m_damage_indicator.enabled )
                m_damage_indicators.push_back( { p.origin + math::vector3{ 0,0,40 }, (float)( hist.health - p.health ), false, now,
                    settings::g_esp.m_damage_indicator.duration / 1000.0f } );
            if ( p.health <= 0 ) m_health_history.erase( p.pawn );
            else { hist.health = p.health; hist.origin = p.origin; }
        }
    }

    void misc_features::fov_changer( )
    {
        if ( !settings::g_misc.m_fov_changer.enabled ) return;
        auto pawn = systems::g_local.pawn( );
        if ( !pawn ) return;
        bool scoped = g::memory.read<bool>( pawn + SCHEMA( "C_CSPlayerPawn", "m_bIsScoped"_hash ) );
        if ( scoped && settings::g_misc.m_fov_changer.disable_when_scoped ) return;
        auto cam = g::memory.read<std::uintptr_t>( pawn + SCHEMA( "C_BasePlayerPawn", "m_pCameraServices"_hash ) );
        if ( !cam ) return;
        g::memory.write<std::uint32_t>( cam + SCHEMA( "CCSPlayerBase_CameraServices", "m_iFOV"_hash ), settings::g_misc.m_fov_changer.fov );
    }

    void misc_features::draw_spectators( zdraw::draw_list& dl )
    {
        if ( !settings::g_misc.m_main.spectator_list ) return;
        std::vector<std::string> specs;
        auto self = systems::g_local.controller( );
        if ( !self ) return;
        for ( auto& e : systems::g_entities.by_type( systems::entities::type::player ) )
        {
            if ( e.ptr == self ) continue;
            bool alive = g::memory.read<bool>( e.ptr + SCHEMA( "CCSPlayerController", "m_bPawnIsAlive"_hash ) );
            if ( alive ) continue;
            auto handle = g::memory.read<std::uint32_t>( e.ptr + SCHEMA( "CCSPlayerController", "m_hObserverPawn"_hash ) );
            if ( !handle || handle == 0xFFFFFFFF ) handle = g::memory.read<std::uint32_t>( e.ptr + SCHEMA( "CCSPlayerController", "m_hPlayerPawn"_hash ) );
            if ( !handle || handle == 0xFFFFFFFF ) continue;
            auto pawn = systems::g_entities.lookup( handle );
            if ( !pawn ) continue;
            auto obs = g::memory.read<std::uintptr_t>( pawn + SCHEMA( "C_BasePlayerPawn", "m_pObserverServices"_hash ) );
            if ( !obs ) continue;
            auto tgt = g::memory.read<std::uint32_t>( obs + SCHEMA( "CPlayer_ObserverServices", "m_hObserverTarget"_hash ) );
            if ( !tgt || tgt == 0xFFFFFFFF ) continue;
            auto tgt_pawn = systems::g_entities.lookup( tgt );
            if ( tgt_pawn != systems::g_local.view_pawn( ) ) continue;
            auto name_ptr = g::memory.read<std::uintptr_t>( e.ptr + SCHEMA( "CCSPlayerController", "m_sSanitizedPlayerName"_hash ) );
            if ( name_ptr ) specs.push_back( g::memory.read_string( name_ptr, 128 ) );
        }
        auto [w, h] = zdraw::get_display_size( );
        float x = 10, y = h / 2;
        dl.add_text( x, y, "spectators", nullptr, settings::g_misc.m_main.spectator_list_color, zdraw::text_style::outlined );
        y += 15;
        for ( auto& s : specs ) { dl.add_text( x, y, s, nullptr, zdraw::rgba{ 195,200,215,230 }, zdraw::text_style::outlined ); y += 15; }
    }

    void misc_features::draw_bomb_timer( zdraw::draw_list& dl )
    {
        if ( !settings::g_misc.m_main.bomb_timer ) return;
        auto& clr = settings::g_misc.m_main.bomb_timer_color;
        for ( auto& bomb : systems::g_entities.by_type( systems::entities::type::bomb ) )
        {
            bool planted = g::memory.read<bool>( bomb.ptr + SCHEMA( "C_PlantedC4", "m_bBombPlanted"_hash ) );
            if ( !planted ) continue;
            bool defused = g::memory.read<bool>( bomb.ptr + SCHEMA( "C_PlantedC4", "m_bBombDefused"_hash ) );
            if ( defused ) continue;
            auto gv = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
            if ( !gv ) continue;
            float now = g::memory.read<float>( gv + 0x30 );
            float blow = g::memory.read<float>( bomb.ptr + SCHEMA( "C_PlantedC4", "m_flC4Blow"_hash ) );
            float left = blow - now;
            if ( left <= 0 ) continue;
            int site = g::memory.read<int>( bomb.ptr + SCHEMA( "C_PlantedC4", "m_nBombSite"_hash ) );
            bool defusing = g::memory.read<bool>( bomb.ptr + SCHEMA( "C_PlantedC4", "m_bBeingDefused"_hash ) );
            float defuse_left = g::memory.read<float>( bomb.ptr + SCHEMA( "C_PlantedC4", "m_flDefuseCountDown"_hash ) ) - now;
            auto [w, h] = zdraw::get_display_size( );
            float cx = w / 2, cy = h * 0.15f;
            std::string txt = std::format( "SITE {} - {:.2f}s", site == 0 ? 'A' : 'B', left );
            float tw, th; std::tie( tw, th ) = zdraw::measure_text( txt );
            dl.add_rect_filled( cx - tw / 2 - 4, cy - 2, tw + 8, 30, zdraw::rgba{ 12,12,12,180 } );
            dl.add_rect( cx - tw / 2 - 4, cy - 2, tw + 8, 30, zdraw::rgba{ 45,45,45,200 } );
            dl.add_text( cx - tw / 2, cy + 2, txt, nullptr, zdraw::rgba{ 215,220,240,255 }, zdraw::text_style::outlined );
            float frac = std::clamp( left / 40.0f, 0.0f, 1.0f );
            auto bar = zui::lerp( zdraw::rgba{ 220,60,60,255 }, clr, frac );
            dl.add_rect_filled( cx - tw / 2, cy + 22, tw, 4, zdraw::rgba{ 30,30,35,200 } );
            dl.add_rect_filled( cx - tw / 2, cy + 22, tw * frac, 4, bar );
            if ( defusing && defuse_left > 0 )
            {
                float total = g::memory.read<float>( bomb.ptr + SCHEMA( "C_PlantedC4", "m_flDefuseLength"_hash ) );
                float dfrac = std::clamp( defuse_left / ( total > 0 ? total : 5 ), 0.0f, 1.0f );
                bool can = defuse_left < left;
                auto dclr = can ? zdraw::rgba{ 90,220,110,255 } : zdraw::rgba{ 220,180,60,255 };
                dl.add_rect_filled( cx - tw / 2, cy + 28, tw, 2, zdraw::rgba{ 30,30,35,200 } );
                dl.add_rect_filled( cx - tw / 2, cy + 28, tw * ( 1 - dfrac ), 2, dclr );
                std::string dtxt = std::format( "DEFUSING - {:.2f}s", defuse_left );
                float dw, dh; std::tie( dw, dh ) = zdraw::measure_text( dtxt );
                dl.add_text( cx - dw / 2, cy + 34, dtxt, nullptr, dclr, zdraw::text_style::outlined );
            }
        }
    }

    void misc_features::hitsounds( )
    {
        auto& cfg = settings::g_misc.m_main;
        if ( cfg.hitsound <= 0 ) return;
        auto pawn = systems::g_local.pawn( );
        if ( !pawn ) return;
        auto bullet = g::memory.read<std::uintptr_t>( pawn + SCHEMA( "C_CSPlayerPawn", "m_pBulletServices"_hash ) );
        if ( !bullet ) return;
        int hits = g::memory.read<int>( bullet + SCHEMA( "CCSPlayer_BulletServices", "m_totalHitsOnServer"_hash ) );
        if ( hits > m_old_hits )
        {
            auto now = std::chrono::steady_clock::now( );
            std::unique_lock lock( m_mutex );
            switch ( cfg.hitsound )
            {
            case 1: PlaySoundA( (LPCSTR)hit_sound, nullptr, SND_MEMORY | SND_ASYNC ); break;
            case 2: PlaySoundA( "SystemAsterisk", nullptr, SND_ALIAS | SND_ASYNC ); break;
            case 3: PlaySoundA( "SystemHand", nullptr, SND_ALIAS | SND_ASYNC ); break;
            }
            if ( settings::g_esp.m_hit_marker.enabled )
                m_hit_markers.push_back( { now, settings::g
