#pragma once

namespace features::skinchanger {

    using weapons_enum = std::uint16_t;

    struct skin_info
    {
        int paint_kit;
        bool uses_old_model;
        std::string name;
        weapons_enum weapon_type;
    };

    struct music_kit_info
    {
        int id;
        std::string name;
    };

    class skin_db
    {
    public:
        void initialize( );
        std::vector<skin_info> get_weapon_skins( weapons_enum type = 0 );
        std::vector<music_kit_info> get_music_kits( );

    private:
        std::vector<skin_info> m_skins;
    };

    class skinchanger
    {
    public:
        void tick( );
        bool m_force_update{ false };
        std::uintptr_t regen_skins{ 0 };
    };

    inline skin_db g_skindb;
    inline skinchanger g_skinchanger;

}
