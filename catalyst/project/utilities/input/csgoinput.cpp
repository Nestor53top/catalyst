#include <stdafx.hpp>

void csgoinput::set_silent_view_angles( const math::vector3& angles ) const
{
    const auto last_frame = this->get_last_frame( );
    if ( !last_frame ) return;
    g::memory.write<float>( last_frame + 0x10, angles.x );
    g::memory.write<float>( last_frame + 0x14, angles.y );
    g::memory.write<float>( last_frame + 0x18, angles.z );
}

int csgoinput::get_frame_count( ) const
{
    const auto input_ptr = g::memory.read<std::uintptr_t>( g::offsets.csgo_input );
    return input_ptr ? g::memory.read<int>( input_ptr + 0xBF0 ) : 0;
}

std::uintptr_t csgoinput::get_frame_history( ) const
{
    const auto input_ptr = g::memory.read<std::uintptr_t>( g::offsets.csgo_input );
    return input_ptr ? g::memory.read<std::uintptr_t>( input_ptr + 0xBF8 ) : 0;
}

std::uintptr_t csgoinput::get_last_frame( ) const
{
    const auto frame_count = this->get_frame_count( );
    if ( frame_count <= 0 ) return 0;
    const auto frame_history_ptr = this->get_frame_history( );
    return frame_history_ptr ? frame_history_ptr + 96 * ( static_cast< std::uintptr_t >( frame_count ) - 1 ) : 0;
}

std::uintptr_t csgoinput::get_frame( int index ) const
{
    const auto frame_count = this->get_frame_count( );
    if ( index < 0 || index >= frame_count ) return 0;
    const auto frame_history_ptr = this->get_frame_history( );
    return frame_history_ptr ? frame_history_ptr + 96 * static_cast< std::uintptr_t >( index ) : 0;
}
