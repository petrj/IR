-- SQLite schema for the IR remote editor.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS devices (
    id   INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS buttons (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id   INTEGER NOT NULL,
    name        TEXT NOT NULL,
    pulse_data  TEXT NOT NULL DEFAULT '[]', -- JSON array of pulse integers
    grid_row    INTEGER NOT NULL DEFAULT 1,
    grid_column INTEGER NOT NULL DEFAULT 1,
    width       INTEGER NOT NULL DEFAULT 1,
    height      INTEGER NOT NULL DEFAULT 1,
    shape       TEXT NOT NULL DEFAULT 'square',
    color       TEXT NOT NULL DEFAULT '#3949ab',
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- Configured IR transmitters (kept separate from the remote definitions).
CREATE TABLE IF NOT EXISTS transmitters (
    id   INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    url  TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_buttons_device ON buttons(device_id);
