#include <stdafx.hpp>

namespace {
    bool ray_hits_capsule( const math::vector3& ray_origin, const math::vector3& ray_dir,
        const math::vector3& capsule_start, const math::vector3& capsule_end, float radius )
    {
        const auto capsule_vec = capsule_end - capsule_start;
        const auto capsule_length = capsule_vec.length( );
        if ( capsule_length < 0.001f )
        {
            const auto to_center = capsule_start - ray_origin;
            const auto proj = to_center.dot( ray_dir );
            if ( proj < 0.0f ) return false;
            const auto closest = ray_origin + ray_dir * proj;
            return ( closest - capsule_start ).length_sqr( ) <= radius * radius;
        }
        const auto capsule_dir = capsule_vec / capsule_length;
        const auto w = ray_origin - capsule_start;
        const auto a = ray_dir.dot( ray_dir );
        const auto b = ray_dir.dot( capsule_dir );
        const auto c = capsule_dir.dot( capsule_dir );
        const auto d = ray_dir.dot( w );
        const auto e = capsule_dir.dot( w );
        const auto denom = a * c - b * b;
        float s, t;
        if ( std::abs( denom ) < 0.0001f )
        {
            s = 0.0f;
            t = ( b > c ? d / b : e / c );
        }
        else
        {
            s = ( b * e - c * d ) / denom;
            t = ( a * e - b * d ) / denom;
        }
        t = std::clamp( t, 0.0f, capsule_length );
        if ( s < 0.0f ) return false;
        const auto point_on_capsule = capsule_start + capsule_dir * t;
        const auto point_on_ray = ray_origin + ray_dir * s;
        return ( point_on_ray - point_on_capsule ).length_sqr( ) <= radius * radius;
    }
}

namespace features::combat {

    void legit::on_render( zdraw::draw_list& dl )
    {
        const auto eye_pos = systems::g_local.eye_position( );
        const auto view_angles = systems::g_view.angles( );
        const auto& ctx = g_shared.ctx( );
        if ( !ctx.valid ) return;
        const auto valid_weapon = cstypes::is_weapon_valid( ctx.weapon_type );
        const auto& cfg = settings::g_combat.get( ctx.weapon_type );

        m_fov_alpha.set_target( valid_weapon && cfg.aimbot.draw_fov && cfg.aimbot.enabled ? 1.0f : 0.0f );
        m_fov_alpha.update( );
        if ( m_fov_alpha.value( ) > 0.01f )
        {
            float rad = get_fov_radius( eye_pos, view_angles, (float)cfg.aimbot.fov );
            rad *= m_fov_alpha.value( );
            if ( rad > 0.5f )
            {
                auto [w, h] = zdraw::get_display_size( );
                auto col = zdraw::rgba{ cfg.aimbot.fov_color.r, cfg.aimbot.fov_color.g, cfg.aimbot.fov_color.b, (std::uint8_t)( m_fov_alpha.value( ) * 125 ) };
                dl.add_circle( w * 0.5f, h * 0.5f, rad, col, 32 );
            }
        }

        if ( valid_weapon && cfg.other.penetration_crosshair )
            draw_penetration_crosshair( dl, systems::g_local.eye_position( ), view_angles, cfg );

        if ( cfg.triggerbot.show_spread && ctx.weapon && ctx.weapon_vdata )
        {
            auto [w, h] = zdraw::get_display_size( );
            float cx = w * 0.5f, cy = h * 0.5f;
            float fov_rad = systems::g_view.fov( ) * std::numbers::pi_v<float> / 180.0f;
            float px_per_rad = (float)w / ( 2.0f * std::tanf( fov_rad * 0.5f ) );
            float inac_px = ctx.inaccuracy * px_per_rad;
            if ( inac_px > 1.0f && inac_px < cx )
                dl.add_circle( cx, cy, inac_px, zdraw::rgba{ 200,200,220,60 }, 64, 1.0f );
            float spread_px = ctx.spread * px_per_rad;
            if ( spread_px > 1.0f && spread_px < cx )
                dl.add_circle( cx, cy, spread_px, zdraw::rgba{ 180,180,200,45 }, 64, 1.0f );
        }
    }

    void legit::tick( )
    {
        if ( !m_rng_seeded )
        {
            m_rng.seed( (int)std::chrono::steady_clock::now( ).time_since_epoch( ).count( ) & 0x7FFFFFFF );
            m_rng_seeded = true;
        }

        if ( m_trigger_held )
        {
            const auto& ctx = g_shared.ctx( );
            if ( !ctx.valid || ctx.current_time >= m_trigger_release_time )
            {
                g::input.inject_mouse( 0, 0, input::left_up );
                m_trigger_held = false;
            }
        }

        const auto& ctx = g_shared.ctx( );
        if ( !ctx.valid ) return;
        if ( !cstypes::is_weapon_valid( ctx.weapon_type ) ) return;

        const auto& cfg = settings::g_combat.get( ctx.weapon_type );
        if ( ctx.is_reloading || !ctx.weapon_ready ) return;

        const auto eye_pos = systems::g_local.eye_position( );
        const auto view_angles = g::memory.read<math::vector3>( systems::g_local.pawn( ) + SCHEMA( "C_BasePlayerPawn", "v_angle"_hash ) );
        const auto camera_angles = systems::g_view.angles( );
        const auto players = systems::g_collector.players( );

        if ( cfg.aimbot.enabled )
        {
            const auto target = select_target( eye_pos, view_angles, players, cfg );
            if ( target.player && ( GetAsyncKeyState( cfg.aimbot.key ) & 0x8000 ) )
                aimbot( eye_pos, camera_angles, target, cfg.aimbot );
        }

        if ( cfg.triggerbot.enabled )
            triggerbot( eye_pos, view_angles, camera_angles, players, cfg.triggerbot, cfg );
    }

    legit::target legit::select_target( const math::vector3& eye_pos, const math::vector3& view_angles,
        const std::vector<systems::collector::player>& players, const settings::combat::group_config& cfg ) const
    {
        target best{};
        best.fov = (float)cfg.aimbot.fov;
        for ( const auto& p : players )
        {
            if ( !systems::g_local.is_enemy( p.team ) || !p.alive ) continue;
            if ( p.invulnerable || p.hitboxes.count <= 0 ) continue;
            const auto bones = systems::g_bones.get( p.bone_cache );
            if ( !bones.is_valid( ) ) continue;
            float dmg = 0; int bone = -1, hg = -1; math::vector3 off; bool pen = false;
            const auto aim = get_aim_point( eye_pos, p, bones, cfg, dmg, bone, off, hg, pen );
            if ( bone < 0 ) continue;
            float fov = get_fov( view_angles, eye_pos, aim );
            if ( fov > best.fov ) continue;
            best.player = &p;
            best.bones = bones;
            best.aim_point = aim;
            best.bone = bone;
            best.offset = off;
            best.hitgroup = hg;
            best.damage = dmg;
            best.fov = fov;
            best.penetrated = pen;
        }
        return best;
    }

    math::vector3 legit::get_aim_point( const math::vector3& eye_pos, const systems::collector::player& player,
        const systems::bones::data& bones, const settings::combat::group_config& cfg,
        float& out_damage, int& out_bone, math::vector3& out_offset, int& out_hitgroup, bool& out_penetrated ) const
    {
        out_bone = -1;
        auto is_hg_enabled = [&]( int hg ) -> bool {
            switch( hg ) {
                case 1: return cfg.aimbot.hitgroups.head;
                case 2: return cfg.aimbot.hitgroups.chest;
                case 3: return cfg.aimbot.hitgroups.stomach;
                case 6: case 7: return cfg.aimbot.hitgroups.arms;
                case 4: case 5: return cfg.aimbot.hitgroups.legs;
                default: return false;
            }
        };
        const auto view = systems::g_view.angles( );
        float best_fov = (float)cfg.aimbot.fov;
        math::vector3 best_point{};
        for ( const auto& hb : player.hitboxes )
        {
            if ( hb.index < 0 || hb.bone < 0 ) continue;
            int hg = systems::g_hitboxes.hitgroup_from_hitbox( hb.index );
            if ( !is_hg_enabled( hg ) ) continue;
            const auto bone_pos = bones.get_position( hb.bone );
            const auto bone_rot = bones.get_rotation( hb.bone );
            const auto local_center = ( hb.mins + hb.maxs ) * 0.5f;
            const auto center = bone_pos + bone_rot.rotate_vector( local_center );
            std::vector<math::vector3> pts = { center };
            if ( cfg.aimbot.multipoint )
            {
                float r = hb.radius * cfg.aimbot.multipoint_scale;
                if ( r > 0.05f )
                {
                    const math::vector3 right = bone_rot.rotate_vector( { 0,1,0 } );
                    const math::vector3 up = bone_rot.rotate_vector( { 0,0,1 } );
                    pts.push_back( center + right * r );
                    pts.push_back( center - right * r );
                    pts.push_back( center + up * r );
                    pts.push_back( center - up * r );
                    if ( hg == 1 )
                    {
                        pts.push_back( center + ( right + up ).normalized( ) * r );
                        pts.push_back( center - ( right + up ).normalized( ) * r );
                    }
                }
            }
            for ( const auto& pos : pts )
            {
                if ( cfg.aimbot.smoke_check && systems::g_voxels.line_goes_through_smoke( eye_pos, pos ) ) continue;
                float dmg = 0; bool pen = false, valid = false;
                if ( !cfg.aimbot.visible_only )
                {
                    dmg = combat::g_shared.pen( ).get_max_damage( hg, player.armor, player.has_helmet, player.team );
                    valid = true;
                }
                else
                {
                    auto trace = systems::g_bvh.trace_ray( eye_pos, pos );
                    if ( !trace.hit || trace.fraction > 0.97f )
                    {
                        dmg = combat::g_shared.pen( ).get_max_damage( hg, player.armor, player.has_helmet, player.team );
                        valid = true;
                    }
                    else if ( cfg.aimbot.autowall )
                    {
                        shared::penetration::result res;
                        if ( combat::g_shared.pen( ).run( eye_pos, pos, player, bones, res ) && res.damage >= cfg.aimbot.min_damage )
                        {
                            dmg = res.damage;
                            pen = res.penetrated;
                            valid = true;
                        }
                    }
                }
                if ( valid )
                {
                    float fov = get_fov( view, eye_pos, pos );
                    if ( fov < best_fov )
                    {
                        best_fov = fov;
                        best_point = pos;
                        out_damage = dmg;
                        out_bone = hb.bone;
                        out_offset = pos - bone_pos;
                        out_hitgroup = hg;
                        out_penetrated = pen;
                    }
                }
            }
        }
        return best_point;
    }

    float legit::get_fov( const math::vector3& view, const math::vector3& eye, const math::vector3& target ) const
    {
        return math::helpers::calculate_fov( view, eye, target );
    }

    float legit::get_fov_radius( const math::vector3& eye, const math::vector3& view, float fov_deg ) const
    {
        if ( fov_deg <= 0 ) return 0;
        math::vector3 fwd, off_fwd;
        view.to_directions( &fwd, nullptr, nullptr );
        auto off = view;
        off.x -= fov_deg;
        off.to_directions( &off_fwd, nullptr, nullptr );
        auto c = systems::g_view.project( eye + fwd * 1000.0f );
        auto e = systems::g_view.project( eye + off_fwd * 1000.0f );
        if ( !systems::g_view.projection_valid( c ) || !systems::g_view.projection_valid( e ) ) return 0;
        float dx = e.x - c.x, dy = e.y - c.y;
        return std::sqrtf( dx * dx + dy * dy );
    }

    void legit::draw_penetration_crosshair( zdraw::draw_list& dl, const math::vector3& eye, const math::vector3& view, const settings::combat::group_config& cfg )
    {
        math::vector3 fwd;
        view.to_directions( &fwd, nullptr, nullptr );
        auto hit = systems::g_bvh.trace_ray( eye, eye + fwd * g_shared.pen( ).get_weapon_data( ).range );
        if ( !hit.hit ) return;
        float pen_dmg = 0;
        bool can = g_shared.pen( ).can( eye, fwd, pen_dmg );
        const auto& n = hit.normal;
        const auto ref = ( std::abs( n.z ) < 0.9f ) ? math::vector3{ 0,0,1 } : math::vector3{ 1,0,0 };
        float d = ref.dot( n );
        auto tangent = ( ref - n * d ).normalized( );
        auto bitangent = n.cross( tangent );
        auto center = hit.end_pos + n * 0.05f;
        constexpr float sz = 3.5f;
        math::vector3 corners[4] = {
            center - tangent * sz - bitangent * sz,
            center + tangent * sz - bitangent * sz,
            center + tangent * sz + bitangent * sz,
            center - tangent * sz + bitangent * sz,
        };
        float sx[5], sy[5];
        for ( int i = 0; i < 4; ++i )
        {
            auto p = systems::g_view.project( corners[i] );
            if ( !systems::g_view.projection_valid( p ) ) return;
            sx[i] = p.x; sy[i] = p.y;
        }
        auto cp = systems::g_view.project( center );
        if ( !systems::g_view.projection_valid( cp ) ) return;
        sx[4] = cp.x; sy[4] = cp.y;
        const auto& col = can ? cfg.other.penetration_color_yes : cfg.other.penetration_color_no;
        auto edge = zdraw::rgba{ col.r, col.g, col.b, (std::uint8_t)( col.a / 4 ) };
        for ( int i = 0; i < 4; ++i )
        {
            int j = ( i + 1 ) % 4;
            dl.add_triangle_filled_multi_color( sx[4], sy[4], sx[i], sy[i], sx[j], sy[j], col, edge, edge );
        }
        float scr[8] = { sx[0], sy[0], sx[1], sy[1], sx[2], sy[2], sx[3], sy[3] };
        dl.add_polyline( std::span<const float>( scr, 8 ), zdraw::rgba{ col.r, col.g, col.b, 255 }, true, 1.0f );
        if ( cfg.other.penetration_damage && can )
        {
            std::string txt = std::format( "{:.0f}", pen_dmg );
            zdraw::push_font( g::render.fonts( ).pretzel_12 );
            auto [tw, th] = zdraw::measure_text( txt );
            dl.add_text( sx[4] - tw * 0.5f, sy[4] + 8, txt, nullptr, zdraw::rgba{ 255,255,255,220 }, zdraw::text_style::outlined );
            zdraw::pop_font( );
        }
    }

    void legit::aimbot( const math::vector3& eye, const math::vector3& view, const target& tgt, const settings::combat::aimbot_settings& cfg )
    {
        HWND fg = GetForegroundWindow( );
        HWND cs2 = FindWindowA( "SDL_app", "Counter-Strike 2" );
        if ( cs2 && fg != cs2 ) { m_aim_error = {}; return; }
        if ( !( GetAsyncKeyState( cfg.key ) & 0x8000 ) ) { m_aim_error = {}; return; }

        constexpr float m_yaw = 0.022f;
        float sens = systems::g_convars.get<float>( CONVAR( "sensitivity"_hash ) );
        float fov_adj = g::memory.read<float>( systems::g_local.pawn( ) + SCHEMA( "C_BasePlayerPawn", "m_flFOVSensitivityAdjust"_hash ) );
        float deg_per_pix = sens * m_yaw * fov_adj;
        if ( deg_per_pix <= 0 ) return;

        const auto fresh = systems::g_bones.get( tgt.player->bone_cache );
        if ( !fresh.is_valid( ) ) return;
        auto aim = fresh.get_position( tgt.bone ) + tgt.offset;
        if ( cfg.predictive )
        {
            float time = g_shared.get_prediction_time( );
            aim += tgt.player->velocity * time;
        }
        auto desired = math::helpers::calculate_angle( eye, aim );
        math::helpers::normalize_angles( desired );

        if ( cfg.silent )
        {
            g::memory.write( systems::g_local.pawn( ) + SCHEMA( "C_BasePlayerPawn", "v_angle"_hash ), desired );
            return;
        }

        if ( cfg.rcs && cfg.rcs_factor > 0 )
        {
            auto punch = g::memory.read<math::vector3>( systems::g_local.pawn( ) + SCHEMA( "C_CSPlayerPawn", "m_aimPunchAngle"_hash ) );
            int shots = g::memory.read<int>( systems::g_local.pawn( ) + SCHEMA( "C_CSPlayerPawn", "m_iShotsFired"_hash ) );
            if ( shots > 1 )
            {
                desired.x -= punch.x * cfg.rcs_factor * 2.0f;
                desired.y -= punch.y * cfg.rcs_factor * 1.5f;
            }
        }

        float dx = desired.x - view.x;
        float dy = math::helpers::normalize_yaw( desired.y - view.y );
        if ( cfg.smoothing > 1 )
        {
            float f = (float)cfg.smoothing;
            dx /= f; dy /= f;
        }
        int move_x = (int)( -dy / deg_per_pix );
        int move_y = (int)( dx / deg_per_pix );
        if ( move_x != 0 || move_y != 0 )
            g::input.inject_mouse( move_x, move_y, input::move );
    }

    legit::trigger_result legit::trace_crosshair( const math::vector3& eye, const math::vector3& view,
        const std::vector<systems::collector::player>& players, const settings::combat::triggerbot_settings& cfg ) const
    {
        trigger_result res{};
        math::vector3 fwd;
        view.to_directions( &fwd, nullptr, nullptr );
        constexpr float max_range = 8192.0f;
        float best_dist = max_range * max_range;
        auto is_hg = [&]( int hg ) -> bool {
            switch( hg ) {
                case 1: return cfg.hitgroups.head;
                case 2: return cfg.hitgroups.chest;
                case 3: return cfg.hitgroups.stomach;
                case 6: case 7: return cfg.hitgroups.arms;
                case 4: case 5: return cfg.hitgroups.legs;
                default: return false;
            }
        };
        for ( const auto& p : players )
        {
            if ( !systems::g_local.is_enemy( p.team ) || !p.alive ) continue;
            if ( p.invulnerable || p.hitboxes.count <= 0 ) continue;
            const auto bones = systems::g_bones.get( p.bone_cache );
            if ( !bones.is_valid( ) ) continue;
            auto& hist = m_prediction_history[ p.pawn ];
            if ( cfg.predictive )
            {
                float time = (float)cfg.predictive_ms * 0.001f;
                auto off = p.velocity * time;
                if ( hist.smoothed_offset.length_sqr( ) == 0 ) hist.smoothed_offset = off;
                else
                {
                    constexpr float tick_dt = 1.0f / 64.0f;
                    hist.smoothed_offset += ( off - hist.smoothed_offset ) * 25.0f * tick_dt;
                }
            }
            else hist.smoothed_offset = {};
            for ( const auto& hb : p.hitboxes )
            {
                if ( hb.index < 0 || hb.bone < 0 ) continue;
                int hg = systems::g_hitboxes.hitgroup_from_hitbox( hb.index );
                if ( !is_hg( hg ) ) continue;
                const auto& bone = bones.bones[ hb.bone ];
                const auto local_center = ( hb.mins + hb.maxs ) * 0.5f;
                auto center = bone.position + bone.rotation.rotate_vector( local_center ) + hist.smoothed_offset;
                float rad = hb.radius;
                if ( !ray_hits_capsule( eye, fwd, center, center, rad ) ) continue;
                float dist_sq = ( center - eye ).length_sqr( );
                if ( dist_sq >= best_dist ) continue;
                if ( systems::g_voxels.line_goes_through_smoke( eye, center ) ) continue;
                auto trace = systems::g_bvh.trace_ray( eye, center );
                if ( !trace.hit || trace.fraction > 0.97f )
                {
                    float dmg = combat::g_shared.pen( ).get_max_damage( hg, p.armor, p.has_helmet, p.team );
                    best_dist = dist_sq;
                    res.player = &p;
                    res.bones = bones;
                    res.hitbox = hb.index;
                    res.hitgroup = hg;
                    res.damage = dmg;
                    res.penetrated = false;
                    res.smoothed_offset = hist.smoothed_offset;
                }
                else if ( cfg.autowall )
                {
                    shared::penetration::result pen_res;
                    if ( combat::g_shared.pen( ).run( eye, center, p, bones, pen_res ) && pen_res.damage >= cfg.min_damage )
                    {
                        best_dist = dist_sq;
                        res.player = &p;
                        res.bones = bones;
                        res.hitbox = pen_res.hitbox;
                        res.hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( pen_res.hitbox );
                        res.damage = pen_res.damage;
                        res.penetrated = pen_res.penetrated;
                        res.smoothed_offset = hist.smoothed_offset;
                    }
                }
            }
        }
        return res;
    }

    void legit::triggerbot( const math::vector3& eye, const math::vector3& view, const math::vector3& cam,
        const std::vector<systems::collector::player>& players, const settings::combat::triggerbot_settings& cfg, const settings::combat::group_config& gcfg )
    {
        HWND fg = GetForegroundWindow( );
        HWND cs2 = FindWindowA( "SDL_app", "Counter-Strike 2" );
        if ( cs2 && fg != cs2 ) { release_autostop( true ); m_trigger_waiting = false; return; }
        if ( !( GetAsyncKeyState( cfg.key ) & 0x8000 ) ) { release_autostop( true ); m_trigger_waiting = false; return; }

        release_autostop( );
        const auto pawn = systems::g_local.pawn( );
        auto vel = pawn ? g::memory.read<math::vector3>( pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) ) : math::vector3{};
        float spd = vel.length_2d( );
        auto fl = pawn ? g::memory.read<std::uint32_t>( pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) ) : 0;
        bool on_ground = ( fl & 1 ) != 0;
        bool moving = spd > 5.0f;

        auto trace = trace_crosshair( eye, cam, players, cfg );
        bool has = trace.player != nullptr;
        if ( has && trace.penetrated && trace.damage < cfg.min_damage ) has = false;

        if ( !has && !cfg.seed_triggerbot ) { m_trigger_waiting = false; return; }

        if ( m_trigger_held ) return;

        if ( cfg.autostop && on_ground && moving && has ) apply_autostop( );

        const auto& ctx = g_shared.ctx( );
        if ( !ctx.weapon_ready ) { m_trigger_waiting = false; return; }

        if ( cfg.hitchance > 0 && has && !cfg.seed_triggerbot )
        {
            float req = cfg.hitchance / 100.0f;
            float hc = g_shared.calculate_hitchance( eye, cam, *trace.player, trace.bones, trace.smoothed_offset );
            if ( hc < req ) return;
        }

        float now = ctx.current_time;
        if ( !m_trigger_waiting )
        {
            m_trigger_waiting = true;
            m_trigger_delay_end = now + (float)cfg.delay * 0.001f;
            return;
        }
        if ( now < m_trigger_delay_end ) return;
        m_trigger_waiting = false;

        float hold = m_rng.random_float( 50.0f, 120.0f );
        g::input.inject_mouse( 0, 0, input::left_down );
        m_trigger_held = true;
        m_trigger_release_time = now + hold * 0.001f;
    }

    void legit::apply_autostop( )
    {
        auto pawn = systems::g_local.pawn( );
        if ( !pawn ) return;
        auto fl = g::memory.read<std::uint32_t>( pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) );
        if ( !( fl & 1 ) ) return;
        auto vel = g::memory.read<math::vector3>( pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );
        if ( vel.length_2d( ) <= 13.0f ) return;
        if ( m_autostop_active ) return;

        auto ang = systems::g_view.angles( );
        math::vector3 fwd, right;
        ang.to_directions( &fwd, &right, nullptr );
        float fv = vel.dot( fwd ), sv = vel.dot( right );
        m_autostop_keys.clear();
        if ( fv > 13.0f ) m_autostop_keys.push_back( 'S' );
        else if ( fv < -13.0f ) m_autostop_keys.push_back( 'W' );
        if ( sv > 13.0f ) m_autostop_keys.push_back( 'A' );
        else if ( sv < -13.0f ) m_autostop_keys.push_back( 'D' );
        if ( m_autostop_keys.empty( ) ) return;
        for ( auto k : m_autostop_keys ) g::input.inject_keyboard( k, true );
        m_autostop_active = true;
        m_autostop_start = std::chrono::steady_clock::now( );
    }

    void legit::release_autostop( bool force )
    {
        if ( !m_autostop_active && !force ) return;
        auto pawn = systems::g_local.pawn( );
        if ( pawn && !force )
        {
            auto vel = g::memory.read<math::vector3>( pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );
            if ( vel.length_2d( ) > 13.0f ) return;
        }
        for ( auto k : m_autostop_keys ) g::input.inject_keyboard( k, false );
        m_autostop_keys.clear();
        m_autostop_active = false;
    }

}
