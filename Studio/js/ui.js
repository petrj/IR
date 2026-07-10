/**
 * Small UI helpers: toast notifications and dialog helpers.
 */

const toastHost = () => document.getElementById('toast-host');

const ICONS = {
    success: 'check_circle',
    error: 'error',
    info: 'info',
};

export function toast(message, type = 'info', duration = 3200) {
    const el = document.createElement('div');
    el.className = `toast toast--${type}`;
    el.innerHTML = `<span class="material-symbols-rounded">${ICONS[type] || ICONS.info}</span><span></span>`;
    el.querySelector('span:last-child').textContent = message;
    toastHost().appendChild(el);

    setTimeout(() => {
        el.classList.add('is-leaving');
        el.addEventListener('animationend', () => el.remove(), { once: true });
    }, duration);
}

/** Open a <dialog> as a modal, tracking it as the topmost dialog. */
export function openDialog(id) {
    const dlg = document.getElementById(id);
    if (dlg && !dlg.open) {
        dlg.showModal();
    }
    return dlg;
}

export function closeDialog(id) {
    const dlg = document.getElementById(id);
    if (dlg && dlg.open) {
        dlg.close();
    }
}

/**
 * Promise-based prompt using the shared #prompt-dialog.
 * Resolves with the entered string, or null if cancelled.
 */
export function prompt({ title, label = 'Value', value = '', message = '', okText = 'OK' }) {
    return new Promise((resolve) => {
        const dlg = document.getElementById('prompt-dialog');
        const q = (sel) => dlg.querySelector(sel);

        q('[data-role="prompt-title"]').textContent = title;
        q('[data-role="prompt-label"]').textContent = label;
        q('[data-role="prompt-ok"]').textContent = okText;

        const msgEl = q('[data-role="prompt-message"]');
        msgEl.textContent = message;
        msgEl.hidden = !message;

        const fieldEl = q('[data-role="prompt-field"]');
        const input = q('[data-role="prompt-input"]');
        const showInput = label !== null;
        fieldEl.hidden = !showInput;
        input.value = value;

        const form = q('[data-role="prompt-form"]');
        const cancelBtn = q('[data-role="prompt-cancel"]');

        const cleanup = (result) => {
            form.removeEventListener('submit', onSubmit);
            cancelBtn.removeEventListener('click', onCancel);
            dlg.removeEventListener('cancel', onCancel);
            if (dlg.open) dlg.close();
            resolve(result);
        };
        const onSubmit = (e) => {
            e.preventDefault();
            cleanup(showInput ? input.value.trim() : true);
        };
        const onCancel = (e) => {
            e.preventDefault();
            cleanup(null);
        };

        form.addEventListener('submit', onSubmit);
        cancelBtn.addEventListener('click', onCancel);
        dlg.addEventListener('cancel', onCancel);

        dlg.showModal();
        if (showInput) {
            input.focus();
            input.select();
        }
    });
}

/** Simple confirm dialog reusing the prompt dialog without a text field. */
export function confirmDialog({ title, message, okText = 'Confirm' }) {
    return prompt({ title, message, label: null, okText });
}
