/**
 * Main application controller: wires together the device list, the virtual
 * remote grid, all dialogs and keyboard shortcuts.
 */

import { api } from './api.js';
import { RemoteGrid } from './remote.js';
import { toast, prompt, confirmDialog, openDialog, closeDialog } from './ui.js';

const CFG = window.APP_CONFIG;

const PRESET_COLORS = ['#9e9e9e', '#212121', '#e53935', '#43a047', '#1e88e5', '#fdd835', '#8e24aa', '#fb8c00'];

// Common remote-control buttons offered in the button dialog so the user rarely
// has to type. Each preset may set the name, colour, shape and size.
const BUTTON_PRESETS = [
    { name: 'Power', color: '#e53935', shape: 'circle', icon: 'power_settings_new' },
    { name: 'Mute', color: '#8e24aa', shape: 'circle', icon: 'volume_off' },
    { name: 'Source', color: '#fb8c00', icon: 'input' },
    { name: 'Menu', icon: 'menu' },
    { name: 'Home', icon: 'home' },
    { name: 'Back', icon: 'arrow_back' },
    { name: 'Exit', icon: 'close' },
    { name: 'OK', color: '#3949ab', shape: 'circle', icon: 'done' },
    { name: 'Up', icon: 'keyboard_arrow_up' },
    { name: 'Down', icon: 'keyboard_arrow_down' },
    { name: 'Left', icon: 'keyboard_arrow_left' },
    { name: 'Right', icon: 'keyboard_arrow_right' },
    { name: 'Vol +', color: '#1e88e5', size: '1x1', icon: 'add' },
    { name: 'Vol -', color: '#1e88e5', size: '1x1', icon: 'remove' },
    { name: 'Ch +', color: '#43a047', icon: 'expand_less' },
    { name: 'Ch -', color: '#43a047', icon: 'expand_more' },
    { name: 'Play', icon: 'play_arrow' },
    { name: 'Pause', icon: 'pause' },
    { name: 'Stop', icon: 'stop' },
    { name: 'Rewind', icon: 'fast_rewind' },
    { name: 'Forward', icon: 'fast_forward' },
    { name: 'Record', color: '#e53935', icon: 'fiber_manual_record' },
    { name: '1' }, { name: '2' }, { name: '3' },
    { name: '4' }, { name: '5' }, { name: '6' },
    { name: '7' }, { name: '8' }, { name: '9' }, { name: '0' },
];

const state = {
    devices: [],
    transmitters: [],
    buttons: [],
    currentDeviceId: null,
    editing: false,
    // Draft used by the button dialog (holds pulses + optional existing id).
    draft: null,
};

let grid;

// ---------------------------------------------------------------------------
// Boot
// ---------------------------------------------------------------------------
init().catch((e) => toast(e.message, 'error'));

async function init() {
    grid = new RemoteGrid(
        document.getElementById('remote-grid'),
        { rows: CFG.gridRows, cols: CFG.gridColumns },
        {
            onButtonActivate: (b) => sendButton(b),
            onButtonEdit: (b) => openActionMenu(b),
            onButtonMove: (b, r, c) => moveButton(b, r, c),
            onInvalidMove: () => toast('That spot is occupied or out of bounds', 'error'),
        }
    );

    buildColorSwatches();
    buildPresets();
    wireToolbar();
    wireButtonDialog();
    wireManualDialog();
    wireSettingsDialog();
    wireSendDialog();
    wirePulseDialog();
    wireCloseButtons();
    wireKeyboard();

    await loadTransmitters();
    await loadDevices();
}

// ---------------------------------------------------------------------------
// Devices
// ---------------------------------------------------------------------------
async function loadDevices(selectId = null) {
    const { devices } = await api.listDevices();
    state.devices = devices;
    renderDeviceList();

    const target = selectId ?? state.currentDeviceId ?? devices[0]?.id ?? null;
    if (target) {
        await selectDevice(target);
    } else {
        state.currentDeviceId = null;
        renderRemote();
    }
}

function renderDeviceList() {
    const list = document.getElementById('device-list');
    const empty = document.getElementById('device-empty');
    list.innerHTML = '';
    empty.hidden = state.devices.length > 0;

    for (const d of state.devices) {
        const li = document.createElement('li');
        li.className = 'device-item' + (d.id === state.currentDeviceId ? ' is-active' : '');
        li.dataset.id = d.id;
        li.setAttribute('role', 'option');
        li.setAttribute('aria-selected', d.id === state.currentDeviceId);
        li.innerHTML = `<span class="material-symbols-rounded">settings_remote</span>
            <span class="device-item__name"></span>`;
        li.querySelector('.device-item__name').textContent = d.name;
        li.addEventListener('click', () => selectDevice(d.id));
        list.appendChild(li);
    }
}

async function selectDevice(id) {
    state.currentDeviceId = id;
    renderDeviceList();
    const { buttons } = await api.listButtons(id);
    state.buttons = buttons;
    renderRemote();
}

function renderRemote() {
    const device = state.devices.find((d) => d.id === state.currentDeviceId);
    document.getElementById('remote-title').textContent = device ? device.name : 'No device selected';
    document.getElementById('remote-hint').hidden = !!device && state.buttons.length > 0;
    grid.setButtons(device ? state.buttons : []);
    grid.setEditing(state.editing);
}

async function newDevice() {
    const name = await prompt({ title: 'New device', label: 'Device name', okText: 'Create' });
    if (!name) return;
    const { device } = await api.createDevice(name);
    await loadDevices(device.id);
    toast(`Device "${device.name}" created`, 'success');
}

async function renameDevice() {
    const device = currentDevice();
    if (!device) return;
    const name = await prompt({ title: 'Rename device', label: 'Device name', value: device.name, okText: 'Save' });
    if (!name) return;
    await api.renameDevice(device.id, name);
    await loadDevices(device.id);
    toast('Device renamed', 'success');
}

async function deleteDevice() {
    const device = currentDevice();
    if (!device) {
        toast('No device selected', 'info');
        return;
    }
    const ok = await confirmDialog({
        title: 'Delete device',
        message: `Delete "${device.name}" and all its buttons? This cannot be undone.`,
        okText: 'Delete',
    });
    if (!ok) return;
    await api.deleteDevice(device.id);
    state.currentDeviceId = null;
    await loadDevices();
    toast('Device deleted', 'success');
}

function currentDevice() {
    return state.devices.find((d) => d.id === state.currentDeviceId) || null;
}

// ---------------------------------------------------------------------------
// Edit mode
// ---------------------------------------------------------------------------
function toggleEdit(force = null) {
    state.editing = force === null ? !state.editing : force;
    const btn = document.querySelector('[data-action="toggle-edit"]');
    btn.dataset.toggle = state.editing ? 'on' : 'off';
    grid.setEditing(state.editing);
}

// ---------------------------------------------------------------------------
// Learning
// ---------------------------------------------------------------------------
let learnAbort = false;

async function startLearn() {
    if (!currentDevice()) {
        const create = await confirmDialog({
            title: 'No device',
            message: 'Create a device first to store learned buttons?',
            okText: 'Create device',
        });
        if (create) await newDevice();
        if (!currentDevice()) return;
    }

    learnAbort = false;
    const dlg = openDialog('learn-dialog');
    document.getElementById('learn-status').textContent =
        'Point your remote at the receiver and press a button.';

    try {
        const res = await api.learn(CFG.learnTimeout);
        if (learnAbort) return;
        closeDialog('learn-dialog');
        if (res.ok && res.pulses?.length) {
            openButtonDialog({ pulses: res.pulses });
            toast(`Captured ${res.count} pulses`, 'success');
        } else {
            toast(res.error || 'No IR signal received', 'error');
        }
    } catch (e) {
        if (!learnAbort) {
            closeDialog('learn-dialog');
            toast(e.message, 'error');
        }
    }
}

// ---------------------------------------------------------------------------
// Manual entry (paste a pulse sequence)
// ---------------------------------------------------------------------------
/** Parse a comma-separated list of positive whole numbers into a pulse array. */
function parsePulses(text) {
    const trimmed = (text || '').trim();
    if (!trimmed) {
        return { error: 'Please paste a pulse sequence.' };
    }
    const parts = trimmed.split(',').map((s) => s.trim()).filter((s) => s !== '');
    if (parts.length === 0) {
        return { error: 'No numbers found. Separate values with commas.' };
    }
    const pulses = [];
    for (const p of parts) {
        if (!/^\d+$/.test(p)) {
            return { error: `"${p}" is not a whole number. Use numbers separated by commas.` };
        }
        const n = Number(p);
        if (!Number.isSafeInteger(n) || n <= 0) {
            return { error: `"${p}" is out of range. Pulses must be positive whole numbers.` };
        }
        pulses.push(n);
    }
    return { pulses };
}

/**
 * Parse a hand-edited IR pulse sequence. Unlike parsePulses() this accepts
 * SIGNED whole numbers, because a real IR signal alternates marks (positive =
 * IR on) and spaces (negative = IR off). Commas, spaces and newlines all work
 * as separators so the field is forgiving to edit.
 */
function parseSignedPulses(text) {
    const trimmed = (text || '').trim();
    if (!trimmed) {
        return { error: 'The IR pulse sequence cannot be empty.' };
    }
    const parts = trimmed.split(/[\s,]+/).filter((s) => s !== '');
    if (parts.length === 0) {
        return { error: 'No numbers found. Separate values with commas.' };
    }
    const pulses = [];
    for (const p of parts) {
        if (!/^-?\d+$/.test(p)) {
            return { error: `"${p}" is not a whole number.` };
        }
        const n = Number(p);
        if (!Number.isSafeInteger(n) || n === 0) {
            return { error: `"${p}" is out of range. Use non-zero whole numbers.` };
        }
        pulses.push(n);
    }
    return { pulses };
}

async function addManual() {
    if (!currentDevice()) {
        const create = await confirmDialog({
            title: 'No device',
            message: 'Create a device first to store the button?',
            okText: 'Create device',
        });
        if (create) await newDevice();
        if (!currentDevice()) return;
    }
    document.getElementById('manual-input').value = '';
    document.getElementById('manual-error').hidden = true;
    openDialog('manual-dialog');
    setTimeout(() => document.getElementById('manual-input').focus(), 50);
}

function wireManualDialog() {
    const form = document.getElementById('manual-form');
    const errEl = document.getElementById('manual-error');
    document.getElementById('manual-cancel').addEventListener('click', () => closeDialog('manual-dialog'));

    form.addEventListener('submit', (e) => {
        e.preventDefault();
        const { pulses, error } = parsePulses(document.getElementById('manual-input').value);
        if (error) {
            errEl.textContent = error;
            errEl.hidden = false;
            return;
        }
        errEl.hidden = true;
        closeDialog('manual-dialog');
        openButtonDialog({ pulses });
        toast(`Parsed ${pulses.length} pulses`, 'success');
    });
}

// ---------------------------------------------------------------------------
// Button dialog (create / edit)
// ---------------------------------------------------------------------------
function buildColorSwatches() {
    const host = document.getElementById('btn-swatches');
    const colorInput = document.getElementById('btn-color');
    host.innerHTML = '';
    for (const c of PRESET_COLORS) {
        const s = document.createElement('button');
        s.type = 'button';
        s.className = 'swatch';
        s.style.background = c;
        s.dataset.color = c;
        s.addEventListener('click', () => {
            colorInput.value = c;
            markActiveSwatch(c);
        });
        host.appendChild(s);
    }
    colorInput.addEventListener('input', () => markActiveSwatch(colorInput.value));
}

function buildPresets() {
    const host = document.getElementById('btn-presets');
    host.innerHTML = '';
    for (const p of BUTTON_PRESETS) {
        const chip = document.createElement('button');
        chip.type = 'button';
        chip.className = 'preset-chip';
        chip.title = `Use "${p.name}"`;
        const icon = p.icon
            ? `<span class="material-symbols-rounded">${p.icon}</span>`
            : '';
        chip.innerHTML = `${icon}<span class="preset-chip__label"></span>`;
        chip.querySelector('.preset-chip__label').textContent = p.name;
        chip.addEventListener('click', () => applyPreset(p));
        host.appendChild(chip);
    }
}

function applyPreset(preset) {
    document.getElementById('btn-name').value = preset.name;
    if (preset.shape) document.getElementById('btn-shape').value = preset.shape;
    if (preset.size) document.getElementById('btn-size').value = preset.size;
    if (preset.color) {
        document.getElementById('btn-color').value = preset.color;
        markActiveSwatch(preset.color);
    }
    document.getElementById('btn-name').focus();
}

function markActiveSwatch(color) {
    document.querySelectorAll('#btn-swatches .swatch').forEach((s) => {
        s.classList.toggle('is-active', s.dataset.color.toLowerCase() === color.toLowerCase());
    });
}

function openButtonDialog(draft) {
    // draft: { id?, name?, pulses, shape?, color?, width?, height?, grid_row?, grid_column? }
    state.draft = { ...draft };
    const isEdit = !!draft.id;

    document.getElementById('button-dialog-title').textContent = isEdit ? 'Edit button' : 'New button';
    document.getElementById('btn-name').value = draft.name || '';
    document.getElementById('btn-shape').value = draft.shape || 'square';
    document.getElementById('btn-color').value = draft.color || '#3949ab';
    markActiveSwatch(draft.color || '#3949ab');

    const w = draft.width || 1;
    const h = draft.height || 1;
    document.getElementById('btn-size').value = `${w}x${h}`;

    // Placement info
    const hint = document.getElementById('btn-placement-hint');
    const val = document.getElementById('btn-placement-value');
    if (draft.grid_row && draft.grid_column) {
        val.hidden = false;
        val.textContent = `Row ${draft.grid_row}, Column ${draft.grid_column}`;
        hint.hidden = true;
    } else {
        val.hidden = true;
        hint.hidden = false;
    }

    document.getElementById('btn-pulses').value = (draft.pulses || []).join(', ');
    const pulseErr = document.getElementById('btn-pulses-error');
    if (pulseErr) pulseErr.hidden = true;
    openDialog('button-dialog');
    setTimeout(() => document.getElementById('btn-name').focus(), 50);
}

function wireButtonDialog() {
    const form = document.getElementById('button-form');
    document.getElementById('button-cancel').addEventListener('click', () => closeDialog('button-dialog'));

    form.addEventListener('submit', async (e) => {
        e.preventDefault();
        const d = state.draft;
        const [w, h] = document.getElementById('btn-size').value.split('x').map(Number);

        // Validate the (possibly hand-edited) IR pulse sequence before saving.
        const pulseErr = document.getElementById('btn-pulses-error');
        const { pulses, error } = parseSignedPulses(document.getElementById('btn-pulses').value);
        if (error) {
            pulseErr.textContent = error;
            pulseErr.hidden = false;
            document.getElementById('btn-pulses-details').open = true;
            return;
        }
        pulseErr.hidden = true;
        d.pulses = pulses;

        const payload = {
            device_id: state.currentDeviceId,
            name: document.getElementById('btn-name').value.trim() || 'Button',
            shape: document.getElementById('btn-shape').value,
            color: document.getElementById('btn-color').value,
            width: w,
            height: h,
            pulse_data: pulses,
        };

        // Position already known (editing an existing button, size unchanged & fits).
        if (d.id && d.grid_row && grid.canPlace(d.grid_row, d.grid_column, w, h, d.id)) {
            payload.id = d.id;
            payload.grid_row = d.grid_row;
            payload.grid_column = d.grid_column;
            closeDialog('button-dialog');
            await api.updateButton(payload);
            await selectDevice(state.currentDeviceId);
            toast('Button saved', 'success');
            return;
        }

        // Need placement: ask the user to click a cell.
        closeDialog('button-dialog');
        toggleEdit(true);
        toast('Click a highlighted cell to place the button', 'info');
        grid.startPlacement(w, h, async (row, col) => {
            payload.grid_row = row;
            payload.grid_column = col;
            if (d.id) {
                payload.id = d.id;
                await api.updateButton(payload);
            } else {
                await api.createButton(payload);
            }
            await selectDevice(state.currentDeviceId);
            toast('Button placed', 'success');
        });
    });
}

// ---------------------------------------------------------------------------
// Button action menu (edit mode)
// ---------------------------------------------------------------------------
let actionButton = null;

function openActionMenu(button) {
    actionButton = button;
    grid.select(button.id);
    document.getElementById('action-title').textContent = button.name;
    openDialog('action-dialog');
}

document.getElementById('action-dialog').addEventListener('click', async (e) => {
    const item = e.target.closest('[data-btn-action]');
    if (!item || !actionButton) return;
    const b = actionButton;
    closeDialog('action-dialog');

    switch (item.dataset.btnAction) {
        case 'send':
            sendButton(b);
            break;
        case 'edit':
            openButtonDialog({
                id: b.id,
                name: b.name,
                pulses: b.pulse_data,
                shape: b.shape,
                color: b.color,
                width: b.width,
                height: b.height,
                grid_row: b.grid_row,
                grid_column: b.grid_column,
            });
            break;
        case 'pulses':
            showPulses(b);
            break;
        case 'relearn':
            await relearn(b);
            break;
        case 'delete':
            await deleteButton(b);
            break;
    }
});

async function relearn(button) {
    learnAbort = false;
    openDialog('learn-dialog');
    try {
        const res = await api.learn(CFG.learnTimeout);
        closeDialog('learn-dialog');
        if (res.ok && res.pulses?.length) {
            await api.updateButton({
                id: button.id,
                device_id: button.device_id,
                name: button.name,
                shape: button.shape,
                color: button.color,
                width: button.width,
                height: button.height,
                grid_row: button.grid_row,
                grid_column: button.grid_column,
                pulse_data: res.pulses,
            });
            await selectDevice(state.currentDeviceId);
            toast('Signal updated', 'success');
        } else {
            toast(res.error || 'No IR signal received', 'error');
        }
    } catch (e) {
        closeDialog('learn-dialog');
        toast(e.message, 'error');
    }
}

async function deleteButton(button) {
    const ok = await confirmDialog({
        title: 'Delete button',
        message: `Delete "${button.name}"?`,
        okText: 'Delete',
    });
    if (!ok) return;
    await api.deleteButton(button.id);
    await selectDevice(state.currentDeviceId);
    toast('Button deleted', 'success');
}

async function moveButton(button, row, col) {
    await api.moveButton(button.id, row, col);
    button.grid_row = row;
    button.grid_column = col;
    grid.render();
}

// ---------------------------------------------------------------------------
// Sending IR
// ---------------------------------------------------------------------------
let sendTarget = null;

function sendButton(button) {
    if (!button.pulse_data?.length) {
        toast('This button has no IR signal', 'error');
        return;
    }
    if (state.transmitters.length === 0) {
        doSend(button, []);
        return;
    }
    // Ask which transmitters to use.
    sendTarget = button;
    const host = document.getElementById('send-targets');
    host.innerHTML = '';
    for (const t of state.transmitters) {
        const label = document.createElement('label');
        label.className = 'tx-check';
        label.innerHTML = `<input type="checkbox" value="${t.id}" checked>
            <span><strong></strong><br><small></small></span>`;
        label.querySelector('strong').textContent = t.name;
        label.querySelector('small').textContent = t.url;
        host.appendChild(label);
    }
    document.getElementById('send-desc').textContent = `Send "${button.name}" to:`;
    openDialog('send-dialog');
}

function wireSendDialog() {
    document.getElementById('send-form').addEventListener('submit', async (e) => {
        e.preventDefault();
        const ids = [...document.querySelectorAll('#send-targets input:checked')].map((i) => +i.value);
        closeDialog('send-dialog');
        if (sendTarget) await doSend(sendTarget, ids);
    });
}

async function doSend(button, transmitterIds) {
    grid.fire(button.id);
    try {
        const res = await api.send(button.pulse_data, transmitterIds, CFG.sendFrequency);
        if (res.ok) {
            const failed = (res.results || []).filter((r) => !r.ok);
            if (failed.length) {
                toast(`Sent, but ${failed.length} target(s) failed`, 'info');
            } else {
                toast(`Sent "${button.name}"`, 'success');
            }
        } else {
            toast(res.error || 'Send failed', 'error');
        }
    } catch (e) {
        toast(e.message, 'error');
    }
}

// ---------------------------------------------------------------------------
// Pulse viewer
// ---------------------------------------------------------------------------
let pulseData = [];

function showPulses(button) {
    pulseData = button.pulse_data || [];
    document.getElementById('pulse-title').textContent = `${button.name} — ${pulseData.length} pulses`;
    renderPulses('json');
    document.querySelectorAll('[data-pulse-view]').forEach((b) =>
        b.classList.toggle('is-active', b.dataset.pulseView === 'json')
    );
    openDialog('pulse-dialog');
}

function renderPulses(mode) {
    const el = document.getElementById('pulse-content');
    if (mode === 'json') {
        el.textContent = JSON.stringify(pulseData);
    } else {
        el.textContent = pulseData.map((v, i) => `${String(i).padStart(3, ' ')}: ${v}`).join('\n');
    }
}

function wirePulseDialog() {
    document.querySelectorAll('[data-pulse-view]').forEach((btn) => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('[data-pulse-view]').forEach((b) => b.classList.remove('is-active'));
            btn.classList.add('is-active');
            renderPulses(btn.dataset.pulseView);
        });
    });
    document.getElementById('pulse-copy').addEventListener('click', async () => {
        try {
            await navigator.clipboard.writeText(JSON.stringify(pulseData));
            toast('Copied to clipboard', 'success');
        } catch {
            toast('Copy failed', 'error');
        }
    });
}

// ---------------------------------------------------------------------------
// Transmitters / settings
// ---------------------------------------------------------------------------
async function loadTransmitters() {
    const { transmitters } = await api.listTransmitters();
    state.transmitters = transmitters;
    renderTransmitterList();
}

function renderTransmitterList() {
    const list = document.getElementById('tx-list');
    const empty = document.getElementById('tx-empty');
    list.innerHTML = '';
    empty.hidden = state.transmitters.length > 0;

    for (const t of state.transmitters) {
        const li = document.createElement('li');
        li.className = 'tx-item';
        li.innerHTML = `<span class="material-symbols-rounded">cast</span>
            <div class="tx-item__info">
                <div class="tx-item__name"></div>
                <div class="tx-item__url"></div>
            </div>
            <button type="button" class="btn btn--icon btn--small" data-tx-del title="Remove">
                <span class="material-symbols-rounded">delete</span>
            </button>`;
        li.querySelector('.tx-item__name').textContent = t.name;
        li.querySelector('.tx-item__url').textContent = t.url;
        li.querySelector('[data-tx-del]').addEventListener('click', async () => {
            await api.deleteTransmitter(t.id);
            await loadTransmitters();
            toast('Transmitter removed', 'success');
        });
        list.appendChild(li);
    }
}

function wireSettingsDialog() {
    document.getElementById('tx-form').addEventListener('submit', async (e) => {
        e.preventDefault();
        const name = document.getElementById('tx-name').value.trim();
        const url = document.getElementById('tx-url').value.trim();
        if (!name || !url) return;
        await api.createTransmitter(name, url);
        document.getElementById('tx-name').value = '';
        document.getElementById('tx-url').value = '';
        await loadTransmitters();
        toast('Transmitter added', 'success');
    });
}

// ---------------------------------------------------------------------------
// Toolbar & generic dialog wiring
// ---------------------------------------------------------------------------
function wireToolbar() {
    const actions = {
        'new-device': newDevice,
        'delete-device': deleteDevice,
        'learn': startLearn,
        'add-manual': addManual,
        'toggle-edit': () => toggleEdit(),
        'settings': () => openDialog('settings-dialog'),
        'help': () => openDialog('help-dialog'),
    };
    document.querySelectorAll('[data-action]').forEach((el) => {
        el.addEventListener('click', () => actions[el.dataset.action]?.());
    });

    // Double-click a device to rename.
    document.getElementById('device-list').addEventListener('dblclick', (e) => {
        if (e.target.closest('.device-item')) renameDevice();
    });

    document.getElementById('learn-cancel').addEventListener('click', () => {
        learnAbort = true;
        closeDialog('learn-dialog');
    });
}

function wireCloseButtons() {
    document.querySelectorAll('[data-close-dialog]').forEach((btn) => {
        btn.addEventListener('click', () => btn.closest('dialog')?.close());
    });
}

// ---------------------------------------------------------------------------
// Keyboard shortcuts
// ---------------------------------------------------------------------------
function wireKeyboard() {
    document.addEventListener('keydown', (e) => {
        // Ignore when typing in a field or when a dialog other than none is focused.
        const tag = e.target.tagName;
        const typing = tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA';
        const dialogOpen = !!document.querySelector('dialog[open]');

        // Global: Escape handled natively by dialogs.
        if (dialogOpen || typing) return;

        switch (e.key.toLowerCase()) {
            case 'n': e.preventDefault(); newDevice(); break;
            case 'l': e.preventDefault(); startLearn(); break;
            case 'm': e.preventDefault(); addManual(); break;
            case 'e': e.preventDefault(); toggleEdit(); break;
            case 's': e.preventDefault(); openDialog('settings-dialog'); break;
            case '?': e.preventDefault(); openDialog('help-dialog'); break;
            case 'delete':
                e.preventDefault();
                deleteDevice();
                break;
            case 'arrowup':
            case 'arrowdown':
                if (document.activeElement.closest('.device-list, .sidebar')) {
                    e.preventDefault();
                    navigateDevices(e.key === 'ArrowUp' ? -1 : 1);
                } else {
                    handleButtonNav(e);
                }
                break;
            case 'arrowleft':
            case 'arrowright':
                handleButtonNav(e);
                break;
            case 'enter':
                if (document.activeElement.classList?.contains('remote-btn')) {
                    e.preventDefault();
                    const id = +document.activeElement.dataset.id;
                    const b = state.buttons.find((x) => x.id === id);
                    if (b) sendButton(b);
                }
                break;
        }
    });
}

function navigateDevices(delta) {
    if (state.devices.length === 0) return;
    const idx = state.devices.findIndex((d) => d.id === state.currentDeviceId);
    const next = Math.max(0, Math.min(state.devices.length - 1, (idx < 0 ? 0 : idx) + delta));
    selectDevice(state.devices[next].id);
}

function handleButtonNav(e) {
    const active = document.activeElement;
    const dirMap = { ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right' };
    const dir = dirMap[e.key];
    if (!dir) return;

    if (active?.classList?.contains('remote-btn')) {
        e.preventDefault();
        const b = grid.neighbour(+active.dataset.id, dir);
        if (b) grid.focusButton(b.id);
    } else if (state.buttons.length) {
        e.preventDefault();
        grid.focusButton(state.buttons[0].id);
    }
}
