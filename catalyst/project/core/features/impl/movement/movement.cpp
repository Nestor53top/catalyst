#include <stdafx.hpp>

namespace features::movement {

    void movement_features::tick( )
    {
        auto pawn = systems::g_local.pawn( );
        if ( !pawn ) return;

        HWND fg = GetForegroundWindow( );
        HWND cs2 = FindWindowA( "SDL_app", "Counter-Strike 2" );
        if ( cs2 && fg != cs2 ) return;

        auto fl = g::memory.read<std::uint32_t>( pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) );
        bool on_ground = ( fl & 1 ) != 0;

        if ( settings::g_movement.bhop.enabled )
        {
            if ( GetAsyncKeyState( VK_SPACE ) & 0x8000 )
            {
                if ( on_ground )
                {
                    g::input.inject_keyboard( VK_F11, false );
                    g::input.inject_keyboard( VK_F11, true );
                    g::input.inject_keyboard( VK_F11, false );
                }
            }
        }

        static bool active = false;
        static std::vector<std::uint16_t> keys;

        if ( settings::g_movement.quickstop.enabled )
        {
            auto vel = g::memory.read<math::vector3>( pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );
            float spd = vel.length_2d( );
            bool w = GetAsyncKeyState( 'W' ) & 0x8000, a = GetAsyncKeyState( 'A' ) & 0x8000, s = GetAsyncKeyState( 'S' ) & 0x8000, d = GetAsyncKeyState( 'D' ) & 0x8000;
            bool manual = w || a || s || d;
            if ( on_ground && spd > ( 13.0f / settings::g_movement.quickstop.strength ) && !manual )
            {
                if ( !active )
                {
                    auto ang = systems::g_view.angles( );
                    math::vector3 fwd, right;
                    ang.to_directions( &fwd, &right, nullptr );
                    float fv = vel.dot( fwd ), sv = vel.dot( right );
                    keys.clear( );
                    if ( std::abs( fv ) > 13 ) keys.push_back( fv > 0 ? 'S' : 'W' );
                    if ( std::abs( sv ) > 13 ) keys.push_back( sv > 0 ? 'A' : 'D' );
                    for ( auto k : keys ) g::input.inject_keyboard( k, true );
                    active = true;
                }
            }
            else if ( active )
            {
                for ( auto k : keys ) g::input.inject_keyboard( k, false );
                keys.clear( );
                active = false;
            }
        }
        else if ( active )
        {
            for ( auto k : keys ) g::input.inject_keyboard( k, false );
            keys.clear( );
            active = false;
        }
    }

}
