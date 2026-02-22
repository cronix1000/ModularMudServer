#pragma once

// ============================================================================
// TAG COMPONENTS
// ============================================================================
// Tag components are empty structs used to mark entities for specific behaviors.
// They have no data, only their presence matters.

// Marks entities that have died
// Used by RespawnSystem to detect mob deaths
struct DeadTag {};

// Marks entities for destruction
// Used by CleanUpSystem to remove entities from the game
struct DestroyTag {};
