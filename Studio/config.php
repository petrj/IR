<?php
/**
 * Central application configuration.
 *
 * All REST API URLs, timeouts and grid dimensions live here so the rest of the
 * application never has to hard-code them.
 */

return [
    // -------------------------------------------------------------------------
    // IR REST backend (Python tiqiaa_IR.py --api)
    // -------------------------------------------------------------------------
    // Default backend used when no transmitter is configured in the database.
    'ir_backend_url' => 'http://127.0.0.1:8080',

    // Endpoint paths on the IR backend (relative to a transmitter base URL).
    'ir_read_path'   => '/api/read',
    'ir_send_path'   => '/api/send',
    'ir_status_path' => '/api/status',

    // -------------------------------------------------------------------------
    // Learning mode
    // -------------------------------------------------------------------------
    // How long (seconds) the backend waits for an IR signal while learning.
    'learn_timeout' => 15,

    // Default carrier frequency (kHz) used when transmitting.
    'send_frequency' => 38,

    // HTTP timeout (seconds) for talking to the IR backend. Must be a little
    // larger than learn_timeout so a slow "read" never gets cut off.
    'http_timeout' => 20,

    // -------------------------------------------------------------------------
    // Virtual remote grid
    // -------------------------------------------------------------------------
    'grid_rows'    => 20,
    'grid_columns' => 3,

    // -------------------------------------------------------------------------
    // Storage
    // -------------------------------------------------------------------------
    'database_path' => __DIR__ . '/database/ir_remote.sqlite',
    'schema_path'   => __DIR__ . '/database/schema.sql',
];
