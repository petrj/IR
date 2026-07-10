/**
 * Thin wrapper around the PHP JSON API (api/index.php).
 * Every method returns the parsed JSON payload or throws on HTTP/transport error.
 */

const BASE = window.APP_CONFIG.apiBase;

async function call(action, { method = 'GET', body = null, query = {} } = {}) {
    const params = new URLSearchParams({ action, ...query });
    const opts = { method, headers: { Accept: 'application/json' } };
    if (body !== null) {
        opts.headers['Content-Type'] = 'application/json';
        opts.body = JSON.stringify(body);
    }
    const res = await fetch(`${BASE}?${params.toString()}`, opts);
    let data;
    try {
        data = await res.json();
    } catch {
        throw new Error(`Invalid server response (HTTP ${res.status})`);
    }
    if (!res.ok || data.error) {
        throw new Error(data.error || `Request failed (HTTP ${res.status})`);
    }
    return data;
}

export const api = {
    config: () => call('config.get'),

    // Devices
    listDevices: () => call('devices.list'),
    createDevice: (name) => call('devices.create', { method: 'POST', body: { name } }),
    renameDevice: (id, name) => call('devices.rename', { method: 'POST', body: { id, name } }),
    deleteDevice: (id) => call('devices.delete', { method: 'POST', body: { id } }),

    // Buttons
    listButtons: (deviceId) => call('buttons.list', { query: { device_id: deviceId } }),
    createButton: (data) => call('buttons.create', { method: 'POST', body: data }),
    updateButton: (data) => call('buttons.update', { method: 'POST', body: data }),
    moveButton: (id, grid_row, grid_column) =>
        call('buttons.move', { method: 'POST', body: { id, grid_row, grid_column } }),
    deleteButton: (id) => call('buttons.delete', { method: 'POST', body: { id } }),

    // Transmitters
    listTransmitters: () => call('transmitters.list'),
    createTransmitter: (name, url) => call('transmitters.create', { method: 'POST', body: { name, url } }),
    updateTransmitter: (id, name, url) => call('transmitters.update', { method: 'POST', body: { id, name, url } }),
    deleteTransmitter: (id) => call('transmitters.delete', { method: 'POST', body: { id } }),

    // IR operations
    learn: (timeout) => call('ir.learn', { query: { timeout } }),
    send: (pulses, transmitterIds, frequency) =>
        call('ir.send', {
            method: 'POST',
            body: { pulses, transmitter_ids: transmitterIds, frequency },
        }),
};
