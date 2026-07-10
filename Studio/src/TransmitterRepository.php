<?php

namespace App;

use PDO;

/**
 * CRUD operations for configured IR transmitters (targets).
 *
 * Transmitters are stored separately from the remote definitions on purpose:
 * a remote button can be sent to any combination of physical IR blasters.
 */
class TransmitterRepository
{
    public function __construct(private PDO $pdo)
    {
    }

    public function all(): array
    {
        return array_map(
            [$this, 'hydrate'],
            $this->pdo->query('SELECT id, name, url FROM transmitters ORDER BY name COLLATE NOCASE')->fetchAll()
        );
    }

    public function find(int $id): ?array
    {
        $stmt = $this->pdo->prepare('SELECT id, name, url FROM transmitters WHERE id = ?');
        $stmt->execute([$id]);
        $row = $stmt->fetch();
        return $row ? $this->hydrate($row) : null;
    }

    /** Resolve a list of transmitter ids into their base URLs. */
    public function urlsFor(array $ids): array
    {
        $ids = array_values(array_filter(array_map('intval', $ids)));
        if (!$ids) {
            return [];
        }
        $placeholders = implode(',', array_fill(0, count($ids), '?'));
        $stmt = $this->pdo->prepare("SELECT url FROM transmitters WHERE id IN ($placeholders)");
        $stmt->execute($ids);
        return array_column($stmt->fetchAll(), 'url');
    }

    public function create(string $name, string $url): array
    {
        $stmt = $this->pdo->prepare('INSERT INTO transmitters (name, url) VALUES (?, ?)');
        $stmt->execute([$name, $url]);
        return ['id' => (int) $this->pdo->lastInsertId(), 'name' => $name, 'url' => $url];
    }

    public function update(int $id, string $name, string $url): void
    {
        $stmt = $this->pdo->prepare('UPDATE transmitters SET name = ?, url = ? WHERE id = ?');
        $stmt->execute([$name, $url, $id]);
    }

    public function delete(int $id): void
    {
        $stmt = $this->pdo->prepare('DELETE FROM transmitters WHERE id = ?');
        $stmt->execute([$id]);
    }

    private function hydrate(array $row): array
    {
        $row['id'] = (int) $row['id'];
        return $row;
    }
}
