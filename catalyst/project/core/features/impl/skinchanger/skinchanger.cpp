#include <stdafx.hpp>

namespace features::skinchanger {

    struct CEconItemAttribute
    {
        std::uintptr_t vtable;
        std::uintptr_t owner;
        char pad[ 32 ];
        std::uint16_t defIndex;
        char pad2[ 2 ];
        float value;
        float initValue;
        std::int32_t refundableCurrency;
        bool setBonus;
        char pad3[ 7 ];
    };

    struct CPtrGameVector
    {
        std::uint64_t size;
        std::uintptr_t ptr;
    };

    static std::uintptr_t get_hud_arms( std::uintptr_t local )
    {
        auto handle = g::memory.read<std::uint32_t>( local + SCHEMA( "C_CSPlayerPawn", "m_hHudModelArms"_hash ) );
        return systems::g_entities.lookup( handle );
    }

    static std::uintptr_t get_hud_weapon( std::uintptr_t local, std::uintptr_t weapon )
    {
        auto arms = get_hud_arms( local );
        auto node = g::memory.read<std::uintptr_t>( arms + SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
        auto child = SCHEMA( "CGameSceneNode", "m_pChild"_hash );
        auto sibling = SCHEMA( "CGameSceneNode", "m_pNextSibling"_hash );
        auto owner = SCHEMA( "CGameSceneNode", "m_pOwner"_hash );
        auto owner_handle = SCHEMA( "C_BaseEntity", "m_hOwnerEntity"_hash );
        for ( auto vm = g::memory.read<std::uintptr_t>( node + child ); vm; vm = g::memory.read<std::uintptr_t>( vm + sibling ) )
        {
            auto ent = g::memory.read<std::uintptr_t>( vm + owner );
            if ( !ent ) continue;
            auto oh = g::memory.read<std::uint32_t>( ent + owner_handle );
            if ( systems::g_entities.lookup( oh ) == weapon ) return ent;
        }
        return 0;
    }

    static void set_mesh_mask( std::uintptr_t ent, std::uint64_t mask )
    {
        if ( !ent ) return;
        auto node = g::memory.read<std::uintptr_t>( ent + SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
        if ( !node ) return;
        auto model_state = node + SCHEMA( "CSkeletonInstance", "m_modelState"_hash );
        auto dirty = g::memory.read<std::uintptr_t>( model_state + 0xD8 );
        if ( dirty ) g::memory.write<std::uint64_t>( dirty + 0x10, mask );
        for ( int i = 0; i < 20; i++ )
        {
            g::memory.write<std::uint64_t>( model_state + SCHEMA( "CModelState", "m_MeshGroupMask"_hash ), mask );
            std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );
        }
    }

    void skinchanger::tick( )
    {
        if ( !settings::g_skinchanger.enabled ) return;

        static bool init = false;
        if ( !init )
        {
            regen_skins = g::memory.find_pattern( g::memory.get_module( "client.dll" ), "48 83 EC ? E8 ? ? ? ? 48 85 C0 0F 84 ? ? ? ? 48 8B 10" );
            g::memory.write<std::uint16_t>( regen_skins + 0x52,
                SCHEMA( "C_EconEntity", "m_AttributeManager"_hash ) +
                SCHEMA( "C_AttributeContainer", "m_Item"_hash ) +
                SCHEMA( "C_EconItemView", "m_AttributeList"_hash ) +
                SCHEMA( "CAttributeList", "m_Attributes"_hash ) );
            init = true;
        }

        auto local = systems::g_local.pawn( );
        if ( !local ) return;
        if ( g::memory.read<std::uint8_t>( local + SCHEMA( "C_BaseEntity", "m_lifeState"_hash ) ) != 0 ) return;

        auto local_ctl = g::memory.read<std::uintptr_t>( g::offsets.local_player_controller );
        if ( local_ctl )
        {
            auto inv = g::memory.read<std::uintptr_t>( local_ctl + SCHEMA( "CBasePlayerController", "m_pInventoryServices"_hash ) );
            if ( inv ) g::memory.write<std::uint16_t>( inv + SCHEMA( "CPlayer_InventoryServices", "m_unMusicID"_hash ), settings::g_skinchanger.music_kit );
        }

        bool need_update = false;
        std::shared_lock lock( settings::g_skinchanger.mutex );
        auto weapons = [&]()
        {
            std::vector<std::uintptr_t> list;
            auto ws = g::memory.read<std::uintptr_t>( local + SCHEMA( "C_BasePlayerPawn", "m_pWeaponServices"_hash ) );
            if ( !ws ) return list;
            auto size = g::memory.read<std::uint64_t>( ws + SCHEMA( "CPlayer_WeaponServices", "m_hMyWeapons"_hash ) );
            auto data = g::memory.read<std::uintptr_t>( ws + SCHEMA( "CPlayer_WeaponServices", "m_hMyWeapons"_hash ) + 8 );
            if ( size > 256 || !data ) return list;
            for ( std::uint32_t i = 0; i < size; ++i )
            {
                auto h = g::memory.read<std::uint32_t>( data + i * 4 );
                if ( !h || h == 0xFFFFFFFF ) continue;
                auto w = systems::g_entities.lookup( h );
                if ( w ) list.push_back( w );
            }
            return list;
        }( );

        for ( auto w : weapons )
        {
            if ( !w ) continue;
            auto item = w + SCHEMA( "C_EconEntity", "m_AttributeManager"_hash ) + SCHEMA( "C_AttributeContainer", "m_Item"_hash );
            if ( m_force_update ) g::memory.write<std::uint32_t>( item + SCHEMA( "C_EconItemView", "m_iItemIDHigh"_hash ), 0 );
            if ( g::memory.read<std::uint32_t>( item + SCHEMA( "C_EconItemView", "m_iItemIDHigh"_hash ) ) == -1 ) continue;
            g::memory.write<std::uint32_t>( item + SCHEMA( "C_EconItemView", "m_iItemIDHigh"_hash ), -1 );

            auto def = g::memory.read<std::uint16_t>( item + SCHEMA( "C_EconItemView", "m_iItemDefinitionIndex"_hash ) );
            auto it = settings::g_skinchanger.weapon_skins.find( def );
            if ( it == settings::g_skinchanger.weapon_skins.end( ) ) continue;
            auto& cfg = it->second;
            if ( cfg.paint_kit == 0 ) continue;

            g::memory.write<std::uint32_t>( w + SCHEMA( "C_EconEntity", "m_nFallbackPaintKit"_hash ), cfg.paint_kit );
            std::uint64_t mask = cfg.uses_old_model + 1;
            set_mesh_mask( w, mask );
            set_mesh_mask( get_hud_weapon( local, w ), mask );

            std::vector<CEconItemAttribute> attrs;
            if ( cfg.paint_kit )
            {
                CEconItemAttribute a{};
                a.defIndex = 6; a.value = (float)cfg.paint_kit; a.initValue = a.value; attrs.push_back( a );
                a.defIndex = 7; a.value = (float)cfg.seed; a.initValue = a.value; attrs.push_back( a );
                a.defIndex = 8; a.value = cfg.wear; a.initValue = a.value; attrs.push_back( a );
            }
            auto attr_list = item + SCHEMA( "C_EconItemView", "m_AttributeList"_hash ) + SCHEMA( "CAttributeList", "m_Attributes"_hash );
            auto pre = g::memory.read<CPtrGameVector>( attr_list );
            if ( !attrs.empty( ) && pre.size == 0 && pre.ptr == 0 )
            {
                auto mem = g::memory.allocate( 0, attrs.size( ) * sizeof( CEconItemAttribute ), PAGE_READWRITE );
                if ( mem )
                {
                    for ( size_t i = 0; i < attrs.size( ); ++i )
                        g::memory.write<CEconItemAttribute>( mem + i * sizeof( CEconItemAttribute ), attrs[ i ] );
                    g::memory.write<CPtrGameVector>( attr_list, { attrs.size( ), mem } );
                }
            }
            need_update = true;
        }

        if ( need_update || m_force_update )
        {
            g::memory.call_remote( regen_skins );
            for ( auto w : weapons )
            {
                if ( g::memory.read<std::uint32_t>( w + SCHEMA( "C_EconEntity", "m_nFallbackPaintKit"_hash ) ) == -1 ) continue;
                g::memory.write<std::uint32_t>( w + SCHEMA( "C_EconEntity", "m_nFallbackPaintKit"_hash ), -1 );
                auto item = w + SCHEMA( "C_EconEntity", "m_AttributeManager"_hash ) + SCHEMA( "C_AttributeContainer", "m_Item"_hash );
                auto attr_list = item + SCHEMA( "C_EconItemView", "m_AttributeList"_hash ) + SCHEMA( "CAttributeList", "m_Attributes"_hash );
                auto pre = g::memory.read<CPtrGameVector>( attr_list );
                if ( pre.size ) { g::memory.write<CPtrGameVector>( attr_list, { 0, 0 } ); g::memory.free( pre.ptr ); }
            }
        }
        m_force_update = false;
    }

}
