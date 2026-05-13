 #include <stdafx.hpp>

void csgoinput::set_silent_view_angles( const math::vector3& angles ) const
{
    auto last = get_last_frame( );
    if ( !last ) return;
    g::memory.write<float>( last + 0x10, angles.x );
    g::memory.write<float>( last + 0x14, angles.y );
    g::memory.write<float>( last + 0x18, angles.z );
}

int csgoinput::get_frame_count( ) const
{
    auto ptr = g::memory.read<std::uintptr_t>( g::offsets.csgo_input );
    return ptr ? g::memory.read<int>( ptr + 0xBF0 ) : 0;
}

std::uintptr_t csgoinput::get_frame_history( ) const
{
    auto ptr = g::memory.read<std::uintptr_t>( g::offsets.csgo_input );
    return ptr ? g::memory.read<std::uintptr_t>( ptr + 0xBF8 ) : 0;
}

std::uintptr_t csgoinput::get_last_frame( ) const
{
    int cnt = get_frame_count( );
    if ( cnt <= 0 ) return 0;
    auto hist = get_frame_history( );
    return hist ? hist + 96 * ( cnt - 1 ) : 0;
}

std::uintptr_t csgoinput::get_frame( int index ) const
{
    int cnt = get_frame_count( );
    if ( index < 0 || index >= cnt ) return 0;
    auto hist = get_frame_history( );
    return hist ? hist + 96 * index : 0;
}
