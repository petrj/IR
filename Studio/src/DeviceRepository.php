<?php

namespace App;

use PDO;

/**
 * CRUD operations for virtual remote-control devices.
 */
class DeviceRepository
{
    public function __construct(private PDO $pdo)
    {
    }

    public function all(): array
    {
        return $this->pdo->query('SELECT id, name FROM devices ORDER BY name COLLATE NOCASE')->fetchAll();
    }

    public function find(int $id): ?array
    {
        $stmt = $this->pdo->prepare('SELECT id, name FROM devices WHERE id = ?');
        $stmt->execute([$id]);
        $row = $stmt->fetch();
        return $row ?: null;
    }

    public function create(string $name): array
    {
        $stmt = $this->pdo->prepare('INSERT INTO devices (name) VALUES (?)');
        $stmt->execute([$name]);
        return ['id' => (int) $this->pdo->lastInsertId(), 'name' => $name];
    }

    public function rename(int $id, string $name): void
    {
        $stmt = $this->pdo->prepare('UPDATE devices SET name = ? WHERE id = ?');
        $stmt->execute([$name, $id]);
    }

    public function delete(int $id): void
    {
        $stmt = $this->pdo->prepare('DELETE FROM devices WHERE id = ?');
        $stmt->execute([$id]);
    }
}
