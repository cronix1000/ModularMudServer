# ModularMudServer - DevOps Setup

The C++ server runs inside Docker. See the repo-root `DEPLOY.md` for the
canonical two-host (prod/beta) deployment workflow.

This file documents the **server-only** operational details: build, env,
DB split, and manual fallback options.

## Build

```bash
docker build -f docker/Dockerfile.server -t mud-server:dev .
```

The build uses `apt-get` to fetch CMake, sol2, nlohmann/json, Lua 5.3, and
SQLite. The output binary is `/mud/ModularMudServer` inside the image.

## Runtime environment

| Variable             | Default                              | Notes                       |
|----------------------|--------------------------------------|-----------------------------|
| `MUD_DB_PATH`        | `/data/mud.world.db`                 | World DB. Mounted volume.   |
| `MUD_PLAYERS_DB`     | derived from `MUD_DB_PATH`           | Players DB. Same dir.       |
| `PORT`               | `27015`                              | Telnet port.                |

The server processes at boot:

1. Opens the world DB at `$MUD_DB_PATH`.
2. ATTACHes the players DB at `$MUD_PLAYERS_DB` under alias `players`.
3. Migrates any orphaned `player_*` rows from `main` into `players` once,
   then DROPs those tables in `main`.
4. Loads world content (items, mobs, regions, …) from the world DB.
5. Listens on `$PORT` for client connections.

## Data layout on the host

```
/opt/mud/
├── docker-compose.yml         # one repo, two profiles
├── .env                       # per-host overrides
├── data/
│   ├── mud.world.db           # design/world state — overwritten each deploy
│   ├── mud.players.db         # player state — preserved across deploys
│   ├── _snapshots/            # .bak files written by safe-db-swap
│   └── backups/               # nightly backups (cron)
└── scripts/
    ├── split-existing-db.mjs  # one-shot split of an old single mud.db
    ├── safe-db-swap.sh        # run on every deploy
    ├── prune-snapshots.sh     # keeps last N .bak files
    └── migrate.mjs            # headless migration runner
```

## Bare-metal fallback (no Docker)

If you ever need to bypass Docker and run the server directly:

```bash
# apt-get install -y build-essential cmake \
#   libsqlite3-dev liblua5.3-dev nlohmann-json3-dev sol2
cd ModularMudServer
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

mkdir -p /tmp/mud-data
cp /opt/mud/data/mud.world.db   /tmp/mud-data/
cp /opt/mud/data/mud.players.db /tmp/mud-data/

MUD_DB_PATH=/tmp/mud-data/mud.world.db \
MUD_PLAYERS_DB=/tmp/mud-data/mud.players.db \
./build/bin/ModularMudServer
```

## Migrations

Migrations are version-controlled in `MudAdmin/server/utils/migrate.ts` and
applied **manually** via the admin UI (`/admin/_migrate`) or via the
`scripts/migrate.mjs` CLI on the host. They are **never** auto-run.

Schema flow:

| Target file                | What's stored                                            |
|----------------------------|----------------------------------------------------------|
| `mud.world.db`             | `world_*` tables + `_migrations` + ad-hoc design data    |
| `mud.players.db`           | `player_*` tables (player accounts, inventories, recipes) |

When a migration touches a `player_*` table, the migration is written to
target `players.<table>` so it lands in the players file.

## Backups

The deploy script (`safe-db-swap.sh`) snapshots the current world DB into
`data/_snapshots/` before swapping in the new one. Players DB is **not**
touched by deploys.

Schedule a nightly player DB backup on each host:

```cron
0 3 * * *  /opt/mud/scripts/backup-players.sh
```

(`/opt/mud/scripts/backup-players.sh` not yet shipped — create on host
using the snippet in DEPLOY.md.)

## Healthcheck

Docker healthcheck is `ss -tln | grep :27015`. Bare-metal:

```bash
ss -tln | grep :27015 || echo "server NOT listening"
```

## Stopping / starting

```bash
# Docker (prod profile)
docker compose --profile prod stop mud-server
docker compose --profile prod start mud-server

# Bare-metal
pkill -TERM -f ModularMudServer
./build/bin/ModularMudServer &
```

## Where the data goes after a deploy

Each container restart re-ATTACHes the same two DB files from the
bind-mounted `/data` directory. World DB is replaced (with snapshot) by
`scripts/safe-db-swap.sh` *before* the container restarts. Players DB is
never modified by the deployment.
