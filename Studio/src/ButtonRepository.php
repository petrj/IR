<?php

namespace App;

use PDO;

/**
 * CRUD operations for buttons that live on a virtual remote.
 */
class ButtonRepository
{
    /** Allowed button shapes. */
    public const SHAPES = ['square', 'circle', 'rounded', 'wide'];

    public function __construct(private PDO $pdo)
    {
    }

    public function forDevice(int $deviceId): array
    {
        $stmt = $this->pdo->prepare(
            'SELECT id, device_id, name, pulse_data, grid_row, grid_column,
                    width, height, shape, color
             FROM buttons WHERE device_id = ? ORDER BY id'
        );
        $stmt->execute([$deviceId]);
        return array_map([$this, 'hydrate'], $stmt->fetchAll());
    }

    public function find(int $id): ?array
    {
        $stmt = $this->pdo->prepare(
            'SELECT id, device_id, name, pulse_data, grid_row, grid_column,
                    width, height, shape, color
             FROM buttons WHERE id = ?'
        );
        $stmt->execute([$id]);
        $row = $stmt->fetch();
        return $row ? $this->hydrate($row) : null;
    }

    public function create(array $data): array
    {
        $stmt = $this->pdo->prepare(
            'INSERT INTO buttons
                (device_id, name, pulse_data, grid_row, grid_column, width, height, shape, color)
             VALUES (:device_id, :name, :pulse_data, :grid_row, :grid_column, :width, :height, :shape, :color)'
        );
        $stmt->execute($this->bind($data));
        return $this->find((int) $this->pdo->lastInsertId());
    }

    public function update(int $id, array $data): ?array
    {
        $stmt = $this->pdo->prepare(
            'UPDATE buttons SET
                name = :name, pulse_data = :pulse_data, grid_row = :grid_row,
                grid_column = :grid_column, width = :width, height = :height,
                shape = :shape, color = :color
             WHERE id = :id'
        );
        $params = $this->bind($data);
        $params[':id'] = $id;
        unset($params[':device_id']);
        $stmt->execute($params);
        return $this->find($id);
    }

    /** Move a single button (used for drag & drop repositioning). */
    public function move(int $id, int $row, int $col): ?array
    {
        $stmt = $this->pdo->prepare('UPDATE buttons SET grid_row = ?, grid_column = ? WHERE id = ?');
        $stmt->execute([$row, $col, $id]);
        return $this->find($id);
    }

    public function delete(int $id): void
    {
        $stmt = $this->pdo->prepare('DELETE FROM buttons WHERE id = ?');
        $stmt->execute([$id]);
    }

    /** Decode the stored JSON pulse column into a PHP array. */
    private function hydrate(array $row): array
    {
        $row['id'] = (int) $row['id'];
        $row['device_id'] = (int) $row['device_id'];
        $row['grid_row'] = (int) $row['grid_row'];
        $row['grid_column'] = (int) $row['grid_column'];
        $row['width'] = (int) $row['width'];
        $row['height'] = (int) $row['height'];
        $row['pulse_data'] = json_decode($row['pulse_data'] ?? '[]', true) ?: [];
        return $row;
    }

    /** Normalise & sanitise incoming values before binding to a statement. */
    private function bind(array $data): array
    {
        $shape = in_array($data['shape'] ?? '', self::SHAPES, true) ? $data['shape'] : 'square';
        $pulses = $data['pulse_data'] ?? [];
        if (is_string($pulses)) {
            $pulses = json_decode($pulses, true) ?: [];
        }
        $pulses = array_values(array_map('intval', (array) $pulses));

        $color = $data['color'] ?? '#3949ab';
        if (!preg_match('/^#[0-9a-fA-F]{6}$/', $color)) {
            $color = '#3949ab';
        }

        return [
            ':device_id'   => (int) ($data['device_id'] ?? 0),
            ':name'        => trim((string) ($data['name'] ?? 'Button')),
            ':pulse_data'  => json_encode($pulses),
            ':grid_row'    => max(1, (int) ($data['grid_row'] ?? 1)),
            ':grid_column' => max(1, (int) ($data['grid_column'] ?? 1)),
            ':width'       => max(1, (int) ($data['width'] ?? 1)),
            ':height'      => max(1, (int) ($data['height'] ?? 1)),
            ':shape'       => $shape,
            ':color'       => $color,
        ];
    }
}
