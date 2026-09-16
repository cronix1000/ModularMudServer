#pragma once
#include <sqlite3.h>
#include <string>

class DatabaseAttach {
public:
    static const char* kWorldDbAlias;
    static const char* kPlayersDbAlias;

    static const char* kPlayerTablesSql;
    static const char* kMigrationsMarkerSql;
    static const char* kMigrationsColumnsList;

    // Opens the players DB file at `playersDbPath` (creating it if missing),
    // ATTACHes it onto `db` under alias `kPlayersDbAlias`, then ensures the
    // `player_*` schema exists in the attached DB. The world DB remains the
    // primary connection (alias "main") and owns the `world_*` and
    // `_migrations` tables.
    //
    // Returns true on success. On failure, the attached DB (if any) is
    // DETACHed and the method returns false.
    static bool AttachPlayers(sqlite3* db, const std::string& playersDbPath);

    // Returns the absolute filesystem path of mud.players.db derived from
    // `worldDbPath`. For example "/data/mud.world.db" -> "/data/mud.players.db".
    // Appends ".players.db" before any existing ".bak.<ISO>" suffix.
    static std::string DerivePlayersPath(const std::string& worldDbPath);

    // Ensures the players DB file exists (creates with schema if missing).
    // Used by the admin loader / split script so the players file is born
    // with the expected tables even when the server isn't running.
    static bool EnsurePlayersFile(const std::string& playersDbPath);
};
