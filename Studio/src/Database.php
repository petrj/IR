<?php

namespace App;

use PDO;

/**
 * Thin PDO/SQLite wrapper that lazily creates the database from schema.sql.
 */
class Database
{
    private PDO $pdo;

    public function __construct(array $config)
    {
        $dbPath = $config['database_path'];
        $dir = dirname($dbPath);
        if (!is_dir($dir)) {
            mkdir($dir, 0775, true);
        }

        $needsSchema = !file_exists($dbPath);

        $this->pdo = new PDO('sqlite:' . $dbPath);
        $this->pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
        $this->pdo->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);
        $this->pdo->exec('PRAGMA foreign_keys = ON');

        if ($needsSchema && file_exists($config['schema_path'])) {
            $this->pdo->exec(file_get_contents($config['schema_path']));
        }
    }

    public function pdo(): PDO
    {
        return $this->pdo;
    }
}
