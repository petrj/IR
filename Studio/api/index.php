<?php
/**
 * JSON REST API for the frontend.
 *
 * A single entry point routes actions passed as ?action=resource.verb.
 * Read/write of local data goes to SQLite; IR read/send is proxied to the
 * Python IR backend via App\IrClient.
 */

declare(strict_types=1);

use App\Database;
use App\DeviceRepository;
use App\ButtonRepository;
use App\TransmitterRepository;
use App\IrClient;

$config = require __DIR__ . '/../bootstrap.php';

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

/** Send a JSON response and stop. */
function respond($data, int $status = 200): never
{
    http_response_code($status);
    echo json_encode($data, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    exit;
}

/** Read and decode a JSON request body (falls back to form data). */
function input(): array
{
    $raw = file_get_contents('php://input');
    if ($raw !== '' && $raw !== false) {
        $decoded = json_decode($raw, true);
        if (is_array($decoded)) {
            return $decoded;
        }
    }
    return $_POST ?: [];
}

try {
    $db = new Database($config);
    $pdo = $db->pdo();
    $devices = new DeviceRepository($pdo);
    $buttons = new ButtonRepository($pdo);
    $transmitters = new TransmitterRepository($pdo);
    $ir = new IrClient($config);

    $action = $_GET['action'] ?? '';
    $body = input();

    switch ($action) {
        // --- Config -----------------------------------------------------------
        case 'config.get':
            respond([
                'grid_rows'      => (int) $config['grid_rows'],
                'grid_columns'   => (int) $config['grid_columns'],
                'learn_timeout'  => (int) $config['learn_timeout'],
                'send_frequency' => (int) $config['send_frequency'],
                'shapes'         => ButtonRepository::SHAPES,
            ]);

        // --- Devices ----------------------------------------------------------
        case 'devices.list':
            respond(['devices' => $devices->all()]);

        case 'devices.create':
            $name = trim((string) ($body['name'] ?? ''));
            if ($name === '') {
                respond(['error' => 'Name is required'], 422);
            }
            respond(['device' => $devices->create($name)]);

        case 'devices.rename':
            $devices->rename((int) ($body['id'] ?? 0), trim((string) ($body['name'] ?? '')));
            respond(['ok' => true]);

        case 'devices.delete':
            $devices->delete((int) ($body['id'] ?? 0));
            respond(['ok' => true]);

        // --- Buttons ----------------------------------------------------------
        case 'buttons.list':
            respond(['buttons' => $buttons->forDevice((int) ($_GET['device_id'] ?? 0))]);

        case 'buttons.create':
            respond(['button' => $buttons->create($body)]);

        case 'buttons.update':
            $btn = $buttons->update((int) ($body['id'] ?? 0), $body);
            respond(['button' => $btn]);

        case 'buttons.move':
            $btn = $buttons->move(
                (int) ($body['id'] ?? 0),
                (int) ($body['grid_row'] ?? 1),
                (int) ($body['grid_column'] ?? 1)
            );
            respond(['button' => $btn]);

        case 'buttons.delete':
            $buttons->delete((int) ($body['id'] ?? 0));
            respond(['ok' => true]);

        // --- Transmitters -----------------------------------------------------
        case 'transmitters.list':
            respond(['transmitters' => $transmitters->all()]);

        case 'transmitters.create':
            $name = trim((string) ($body['name'] ?? ''));
            $url = trim((string) ($body['url'] ?? ''));
            if ($name === '' || $url === '') {
                respond(['error' => 'Name and URL are required'], 422);
            }
            respond(['transmitter' => $transmitters->create($name, $url)]);

        case 'transmitters.update':
            $transmitters->update(
                (int) ($body['id'] ?? 0),
                trim((string) ($body['name'] ?? '')),
                trim((string) ($body['url'] ?? ''))
            );
            respond(['ok' => true]);

        case 'transmitters.delete':
            $transmitters->delete((int) ($body['id'] ?? 0));
            respond(['ok' => true]);

        // --- IR operations ----------------------------------------------------
        case 'ir.learn':
            $timeout = isset($_GET['timeout']) ? (int) $_GET['timeout'] : null;
            respond($ir->read($timeout));

        case 'ir.send':
            $pulses = $body['pulses'] ?? [];
            if (!is_array($pulses) || count($pulses) === 0) {
                // Allow sending a stored button by id.
                if (isset($body['button_id'])) {
                    $btn = $buttons->find((int) $body['button_id']);
                    $pulses = $btn['pulse_data'] ?? [];
                }
            }
            if (!is_array($pulses) || count($pulses) === 0) {
                respond(['ok' => false, 'error' => 'No pulses to send'], 422);
            }
            $urls = $transmitters->urlsFor((array) ($body['transmitter_ids'] ?? []));
            $freq = isset($body['frequency']) ? (int) $body['frequency'] : null;
            respond($ir->send($pulses, $urls, $freq));

        case 'ir.status':
            respond($ir->status());

        default:
            respond(['error' => 'Unknown action: ' . $action], 404);
    }
} catch (\Throwable $e) {
    respond(['error' => $e->getMessage()], 500);
}
