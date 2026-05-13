#include <stdafx.hpp>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace features::skinchanger {

    static size_t write_cb( void* contents, size_t size, size_t nmemb, void* userp )
    {
        ( (std::string*)userp )->append( (char*)contents, size * nmemb );
        return size * nmemb;
    }

    void skin_db::initialize( )
    {
        CURL* curl = curl_easy_init( );
        if ( !curl ) return;
        std::string buf;
        curl_easy_setopt( curl, CURLOPT_URL, "https://raw.githubusercontent.com/ByMykel/CSGO-API/main/public/api/en/skins.json" );
        curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_cb );
        curl_easy_setopt( curl, CURLOPT_WRITEDATA, &buf );
        curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1L );
        CURLcode res = curl_easy_perform( curl );
        curl_easy_cleanup( curl );
        if ( res != CURLE_OK || buf.empty( ) ) return;

        try
        {
            auto json = nlohmann::json::parse( buf );
            if ( !json.is_array( ) ) return;
            for ( auto& skin : json )
            {
                if ( !skin.is_object( ) ) continue;
                skin_info info;
                if ( skin.contains( "paint_index" ) )
                {
                    if ( skin["paint_index"].is_string( ) ) info.paint_kit = std::stoi( skin["paint_index"].get<std::string>( ) );
                    else if ( skin["paint_index"].is_number( ) ) info.paint_kit = skin["paint_index"].get<int>( );
                    else info.paint_kit = 0;
                }
                else info.paint_kit = 0;
                if ( skin.contains( "name" ) )
                {
                    if ( skin["name"].is_string( ) ) info.name = skin["name"].get<std::string>( );
                    else if ( skin["name"].is_object( ) && skin["name"].contains( "en" ) ) info.name = skin["name"]["en"].get<std::string>( );
                    else info.name = "";
                }
                else info.name = "";
                if ( skin.contains( "legacy_model" ) && skin["legacy_model"].is_boolean( ) )
                    info.uses_old_model = skin["legacy_model"].get<bool>( );
                else info.uses_old_model = false;

                auto get_type = []( const std::string& n ) -> weapons_enum {
                    if ( n.find( "AK-47" ) != std::string::npos ) return 7;
                    if ( n.find( "M4A4" ) != std::string::npos ) return 16;
                    if ( n.find( "M4A1-S" ) != std::string::npos ) return 60;
                    if ( n.find( "AWP" ) != std::string::npos ) return 9;
                    if ( n.find( "Desert Eagle" ) != std::string::npos ) return 1;
                    if ( n.find( "USP-S" ) != std::string::npos ) return 61;
                    if ( n.find( "Glock-18" ) != std::string::npos ) return 4;
                    if ( n.find( "Dual Berettas" ) != std::string::npos ) return 2;
                    if ( n.find( "Five-SeveN" ) != std::string::npos ) return 3;
                    if ( n.find( "P2000" ) != std::string::npos ) return 32;
                    if ( n.find( "P250" ) != std::string::npos ) return 36;
                    if ( n.find( "Tec-9" ) != std::string::npos ) return 30;
                    if ( n.find( "CZ75-Auto" ) != std::string::npos ) return 63;
                    if ( n.find( "R8 Revolver" ) != std::string::npos ) return 64;
                    if ( n.find( "MAC-10" ) != std::string::npos ) return 17;
                    if ( n.find( "MP5-SD" ) != std::string::npos ) return 23;
                    if ( n.find( "MP7" ) != std::string::npos ) return 33;
                    if ( n.find( "MP9" ) != std::string::npos ) return 34;
                    if ( n.find( "P90" ) != std::string::npos ) return 19;
                    if ( n.find( "UMP-45" ) != std::string::npos ) return 24;
                    if ( n.find( "PP-Bizon" ) != std::string::npos ) return 26;
                    if ( n.find( "Galil AR" ) != std::string::npos ) return 13;
                    if ( n.find( "FAMAS" ) != std::string::npos ) return 10;
                    if ( n.find( "AUG" ) != std::string::npos ) return 8;
                    if ( n.find( "SG 553" ) != std::string::npos ) return 39;
                    if ( n.find( "G3SG1" ) != std::string::npos ) return 11;
                    if ( n.find( "SCAR-20" ) != std::string::npos ) return 38;
                    if ( n.find( "SSG 08" ) != std::string::npos ) return 40;
                    if ( n.find( "Nova" ) != std::string::npos ) return 35;
                    if ( n.find( "XM1014" ) != std::string::npos ) return 25;
                    if ( n.find( "Sawed-Off" ) != std::string::npos ) return 29;
                    if ( n.find( "MAG-7" ) != std::string::npos ) return 27;
                    if ( n.find( "M249" ) != std::string::npos ) return 14;
                    if ( n.find( "Negev" ) != std::string::npos ) return 28;
                    return 0;
                };
                info.weapon_type = get_type( info.name );
                if ( info.weapon_type != 0 ) m_skins.push_back( info );
            }
        }
        catch ( ... ) { }
    }

    std::vector<skin_info> skin_db::get_weapon_skins( weapons_enum type )
    {
        std::vector<skin_info> res = { { 0, false, "Vanilla", type } };
        for ( const auto& s : m_skins )
            if ( s.weapon_type == type ) res.push_back( s );
        return res;
    }

    std::vector<music_kit_info> skin_db::get_music_kits( )
    {
        return {
            { 0, "Default" }, { 1, "Counter-Strike 2" }, { 3, "Crimson Assault" }, { 6, "A_D_8" }, { 7, "High Noon" },
            { 8, "Death's Head Demolition" }, { 9, "Desert Fire" }, { 11, "Metal" }, { 14, "For No Mankind" },
            { 15, "Hotline Miami" }, { 17, "The Talos Principle" }, { 18, "Battlepack" }, { 19, "Molotov" },
            { 20, "Uber Blasto Phone" }, { 21, "Hazardous Environments" }, { 22, "Headshot" }, { 23, "Total Domination" },
            { 24, "I Am" }, { 25, "Diamonds" }, { 26, "Invasion!" }, { 27, "Lion's Mouth" }, { 28, "Sponge Fingerz" },
            { 29, "Aggressive" }, { 31, "Moments CSGO" }, { 32, "Disgusting" }, { 35, "Life's Not Out To Get You" },
            { 36, "Backbone" }, { 37, "GLA" }, { 38, "Arena" }, { 39, "EZ4ENCE" }, { 40, "Halo" }, { 41, "King, Scar" },
            { 42, "Anti-Citizen" }, { 43, "Bachram" }, { 44, "Taco Truck" }, { 45, "Eye of the Dragon" }, { 46, "M.U.D.D. FORCE" },
            { 47, "Neo Noir" }, { 48, "Bodacious" }, { 49, "Drifter" }, { 50, "All For Dust" }, { 51, "Hades" },
            { 53, "CHAINSAW Lxadxut" }, { 54, "Mocha Petal" }, { 55, "Yellow Magic" }, { 56, "VICI" }, { 57, "Atro Bellum" },
            { 58, "Work Hard, Play Hard" }, { 59, "Kolibri" }, { 60, "U Mad!" }, { 61, "Flashbang Dance" },
            { 62, "Heading For The Source" }, { 63, "Void" }, { 64, "Shooters" }, { 65, "Dashstar*" }, { 66, "Gothic Luxury" },
            { 67, "Lock Me Up" }, { 68, "Hua Lian" }, { 69, "Ultimate" }, { 76, "Under Bright Lights" }, { 84, "CS:GO" }
        };
    }

}
