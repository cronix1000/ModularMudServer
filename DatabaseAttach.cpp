#include "DatabaseAttach.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void ExecOrWarn(sqlite3* db, const char* sql, const char* context) {
    if (!db || !sql) return;
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::fprintf(stderr,
                     "[DatabaseAttach] %s failed (%d): %s\nSQL: %s\n",
                     context ? context : "exec",
                     rc,
                     err ? err : "(no message)",
                     sql);
        sqlite3_free(err);
    }
}

bool FileExists(const std::string& path) {
    if (path.empty()) return false;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

}

const char* DatabaseAttach::kWorldDbAlias   = "main";
const char* DatabaseAttach::kPlayersDbAlias = "players";

const char* DatabaseAttach::kPlayerTablesSql =
    "CREATE TABLE IF NOT EXISTS player_players ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  region_id TEXT DEFAULT 'floor1',"
    "  account_id INTEGER UNIQUE,"
    "  permission INTEGER NOT NULL,"
    "  name TEXT UNIQUE NOT NULL,"
    "  password_hash TEXT NOT NULL,"
    "  salt TEXT NOT NULL,"
    "  room_id INTEGER DEFAULT 1,"
    "  data TEXT NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS player_items ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  owner_id INTEGER NOT NULL,"
    "  template_id TEXT NOT NULL,"
    "  item_state TEXT NOT NULL,"
    "  FOREIGN KEY (owner_id) REFERENCES player_players(id) ON DELETE CASCADE"
    ");"
    "CREATE TABLE IF NOT EXISTS player_known_recipes ("
    "  uid INTEGER NOT NULL,"
    "  world_id TEXT NOT NULL,"
    "  recipe_id TEXT NOT NULL,"
    "  learned_at INTEGER NOT NULL,"
    "  PRIMARY KEY (uid, world_id, recipe_id)"
    ");";

const char* DatabaseAttach::kMigrationsMarkerSql =
    "CREATE TABLE IF NOT EXISTS _migrations ("
    "  version INTEGER PRIMARY KEY,"
    "  name TEXT NOT NULL,"
    "  applied_at INTEGER NOT NULL,"
    "  note TEXT"
    ");";

const char* DatabaseAttach::kMigrationsColumnsList =
    "version INTEGER PRIMARY KEY,"
    "name TEXT NOT NULL,"
    "applied_at INTEGER NOT NULL,"
    "note TEXT";

std::string DatabaseAttach::DerivePlayersPath(const std::string& worldDbPath) {
    if (worldDbPath.empty()) return "mud.players.db";

    const std::string bak = ".bak.";
    const size_t bakPos = worldDbPath.find(bak);
    const std::string base = (bakPos == std::string::npos)
                                 ? worldDbPath
                                 : worldDbPath.substr(0, bakPos);

    const std::string worldTok = ".world.db";
    const size_t wpos = base.rfind(worldTok);
    if (wpos != std::string::npos) {
        return base.substr(0, wpos) + ".players.db" + base.substr(wpos + worldTok.size());
    }
    if (base.size() >= 3 && (base.compare(base.size() - 3, 3, ".db") == 0)) {
        return base.substr(0, base.size() - 3) + ".players.db";
    }
    return base + ".players.db";
}

bool DatabaseAttach::EnsurePlayersFile(const std::string& playersDbPath) {
    if (playersDbPath.empty()) {
        std::fprintf(stderr, "[DatabaseAttach] empty playersDbPath\n");
        return false;
    }
    sqlite3* standalone = nullptr;
    int rc = sqlite3_open(playersDbPath.c_str(), &standalone);
    if (rc != SQLITE_OK) {
        std::fprintf(stderr,
                     "[DatabaseAttach] cannot open players file '%s' (%d): %s\n",
                     playersDbPath.c_str(),
                     rc,
                     standalone ? sqlite3_errmsg(standalone) : "(no handle)");
        if (standalone) sqlite3_close(standalone);
        return false;
    }
    ExecOrWarn(standalone, "PRAGMA foreign_keys = ON;", "enable fk (players standalone)");
    ExecOrWarn(standalone, kPlayerTablesSql,        "create player_* tables (standalone)");
    sqlite3_close(standalone);
    return true;
}

bool DatabaseAttach::AttachPlayers(sqlite3* db, const std::string& playersDbPath) {
    if (!db) {
        std::fprintf(stderr, "[DatabaseAttach] null db handle\n");
        return false;
    }
    if (playersDbPath.empty()) {
        std::fprintf(stderr, "[DatabaseAttach] empty playersDbPath\n");
        return false;
    }

    std::string attachSql = "ATTACH DATABASE '";
    attachSql += playersDbPath;
    attachSql += "' AS ";
    attachSql += kPlayersDbAlias;
    attachSql += ';';
    char* err = nullptr;
    int rc = sqlite3_exec(db, attachSql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::fprintf(stderr,
                     "[DatabaseAttach] ATTACH failed (%d): %s\n",
                     rc,
                     err ? err : "(no message)");
        sqlite3_free(err);
        return false;
    }

    return true;
}

bool DatabaseAttach::OpenAndAttach(sqlite3* db, const std::string& worldDbPath) {
    if (!db) {
        std::fprintf(stderr, "[DatabaseAttach] null db handle\n");
        return false;
    }
    if (worldDbPath.empty()) {
        std::fprintf(stderr, "[DatabaseAttach] empty worldDbPath\n");
        return false;
    }

    std::string playersPath = DerivePlayersPath(worldDbPath);

    if (const char* envBuf = std::getenv("MUD_PLAYERS_DB"); envBuf != nullptr && *envBuf != '\0') {
        playersPath = envBuf;
    }

    if (!FileExists(playersPath)) {
        if (!EnsurePlayersFile(playersPath)) return false;
    }

    if (!AttachPlayers(db, playersPath)) return false;

    std::printf("[Database] Players DB attached: %s\n", playersPath.c_str());
    return true;
}
