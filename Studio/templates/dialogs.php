<?php
/** All modal dialogs. Visibility is toggled from JavaScript. */
?>

<!-- Generic prompt dialog (create / rename device, confirmations) -->
<dialog id="prompt-dialog" class="dialog">
    <form method="dialog" class="dialog__form" data-role="prompt-form">
        <h3 class="dialog__title" data-role="prompt-title">Title</h3>
        <p class="dialog__text" data-role="prompt-message" hidden></p>
        <div class="field" data-role="prompt-field">
            <label class="field__label" data-role="prompt-label">Value</label>
            <input type="text" class="input" data-role="prompt-input" autocomplete="off">
        </div>
        <div class="dialog__actions">
            <button type="button" class="btn btn--text" data-role="prompt-cancel">Cancel</button>
            <button type="submit" class="btn btn--primary" data-role="prompt-ok">OK</button>
        </div>
    </form>
</dialog>

<!-- Button editor (used for both Learn placement and later editing) -->
<dialog id="button-dialog" class="dialog dialog--wide">
    <form method="dialog" class="dialog__form" id="button-form">
        <h3 class="dialog__title" id="button-dialog-title">Button</h3>

        <div class="field">
            <label class="field__label">Quick presets</label>
            <div class="preset-grid" id="btn-presets"></div>
        </div>

        <div class="dialog__grid">
            <div class="field">
                <label class="field__label" for="btn-name">Name</label>
                <input type="text" id="btn-name" class="input" required autocomplete="off">
            </div>

            <div class="field">
                <label class="field__label" for="btn-shape">Shape</label>
                <select id="btn-shape" class="input">
                    <option value="square">Square</option>
                    <option value="circle">Circle</option>
                    <option value="rounded">Rounded rectangle</option>
                    <option value="wide">Wide rectangle</option>
                </select>
            </div>

            <div class="field">
                <label class="field__label" for="btn-size">Size (width × height)</label>
                <select id="btn-size" class="input">
                    <option value="1x1">1 × 1</option>
                    <option value="2x1">2 × 1 (wide)</option>
                    <option value="3x1">3 × 1 (wide)</option>
                    <option value="2x2">2 × 2</option>
                    <option value="1x2">1 × 2 (tall)</option>
                    <option value="1x3">1 × 3 (tall)</option>
                </select>
            </div>

            <div class="field">
                <label class="field__label" for="btn-color">Color</label>
                <div class="color-row">
                    <input type="color" id="btn-color" class="input input--color" value="#3949ab">
                    <div class="swatches" id="btn-swatches"></div>
                </div>
            </div>
        </div>

        <div class="field">
            <label class="field__label">Placement</label>
            <p class="field__hint" id="btn-placement-hint">
                Close this dialog and click an empty cell in the grid to place the button.
            </p>
            <p class="field__value" id="btn-placement-value" hidden></p>
        </div>

        <details class="field" id="btn-pulses-details">
            <summary class="field__label">IR pulse sequence</summary>
            <p class="field__hint">
                Whole numbers separated by commas. Positive = mark (IR on),
                negative = space (IR off). Editing this changes the exact signal that is transmitted.
            </p>
            <textarea id="btn-pulses" class="input input--area" rows="6"
                autocomplete="off" spellcheck="false"></textarea>
            <p class="field__error" id="btn-pulses-error" hidden></p>
        </details>

        <div class="dialog__actions">
            <button type="button" class="btn btn--text" id="button-cancel">Cancel</button>
            <button type="submit" class="btn btn--primary" id="button-save">Save</button>
        </div>
    </form>
</dialog>

<!-- Manual pulse entry -->
<dialog id="manual-dialog" class="dialog dialog--wide">
    <form method="dialog" class="dialog__form" id="manual-form">
        <h3 class="dialog__title">Add button manually</h3>
        <p class="dialog__text">
            Paste the IR pulse sequence as whole numbers separated by commas
            (for example <code>9000, 4500, 560, 1690, 560</code>).
        </p>
        <div class="field">
            <label class="field__label" for="manual-input">Pulse sequence</label>
            <textarea id="manual-input" class="input input--area" rows="6"
                placeholder="9000, 4500, 560, 1690, 560, 560, ..." autocomplete="off" spellcheck="false"></textarea>
        </div>
        <p class="field__error" id="manual-error" hidden></p>
        <div class="dialog__actions">
            <button type="button" class="btn btn--text" id="manual-cancel">Cancel</button>
            <button type="submit" class="btn btn--primary">Continue</button>
        </div>
    </form>
</dialog>

<!-- Button context menu / actions (shown when a button is selected in edit mode) -->
<dialog id="action-dialog" class="dialog">
    <div class="dialog__form">
        <h3 class="dialog__title" id="action-title">Button</h3>
        <div class="action-list">
            <button type="button" class="action-item" data-btn-action="send">
                <span class="material-symbols-rounded">send</span> Send IR
            </button>
            <button type="button" class="action-item" data-btn-action="edit">
                <span class="material-symbols-rounded">edit</span> Edit
            </button>
            <button type="button" class="action-item" data-btn-action="pulses">
                <span class="material-symbols-rounded">graphic_eq</span> View pulses
            </button>
            <button type="button" class="action-item" data-btn-action="relearn">
                <span class="material-symbols-rounded">sensors</span> Re-learn signal
            </button>
            <button type="button" class="action-item action-item--danger" data-btn-action="delete">
                <span class="material-symbols-rounded">delete</span> Delete
            </button>
        </div>
        <div class="dialog__actions">
            <button type="button" class="btn btn--text" data-close-dialog>Close</button>
        </div>
    </div>
</dialog>

<!-- Settings: IR transmitters -->
<dialog id="settings-dialog" class="dialog dialog--wide">
    <div class="dialog__form">
        <h3 class="dialog__title">IR Transmitters</h3>
        <p class="dialog__text">
            Configure one or more target IR blasters. When sending, you can pick any combination.
        </p>

        <ul class="tx-list" id="tx-list"></ul>
        <p class="sidebar__empty" id="tx-empty" hidden>No transmitters configured. The default backend will be used.</p>

        <form class="tx-form" id="tx-form">
            <input type="text" class="input" id="tx-name" placeholder="Name (e.g. Living room)" required>
            <input type="url" class="input" id="tx-url" placeholder="http://127.0.0.1:8080" required>
            <button type="submit" class="btn btn--tonal">
                <span class="material-symbols-rounded">add</span> Add
            </button>
        </form>

        <div class="dialog__actions">
            <button type="button" class="btn btn--primary" data-close-dialog>Done</button>
        </div>
    </div>
</dialog>

<!-- Send target picker -->
<dialog id="send-dialog" class="dialog">
    <form method="dialog" class="dialog__form" id="send-form">
        <h3 class="dialog__title">Send IR</h3>
        <p class="dialog__text" id="send-desc">Choose the target transmitters.</p>
        <div class="tx-picker" id="send-targets"></div>
        <div class="dialog__actions">
            <button type="button" class="btn btn--text" data-close-dialog>Cancel</button>
            <button type="submit" class="btn btn--primary">Send</button>
        </div>
    </form>
</dialog>

<!-- Pulse viewer -->
<dialog id="pulse-dialog" class="dialog dialog--wide">
    <div class="dialog__form">
        <h3 class="dialog__title" id="pulse-title">IR sequence</h3>
        <div class="segmented" role="tablist">
            <button type="button" class="segmented__btn is-active" data-pulse-view="json">JSON</button>
            <button type="button" class="segmented__btn" data-pulse-view="list">List</button>
        </div>
        <pre class="pulse-view pulse-view--tall" id="pulse-content"></pre>
        <div class="dialog__actions">
            <button type="button" class="btn btn--text" id="pulse-copy">
                <span class="material-symbols-rounded">content_copy</span> Copy
            </button>
            <button type="button" class="btn btn--primary" data-close-dialog>Close</button>
        </div>
    </div>
</dialog>

<!-- Learning overlay -->
<dialog id="learn-dialog" class="dialog">
    <div class="dialog__form dialog__form--center">
        <div class="learn-spinner"><span class="material-symbols-rounded">sensors</span></div>
        <h3 class="dialog__title">Waiting for IR signal…</h3>
        <p class="dialog__text" id="learn-status">Point your remote at the receiver and press a button.</p>
        <div class="dialog__actions">
            <button type="button" class="btn btn--text" id="learn-cancel">Cancel</button>
        </div>
    </div>
</dialog>

<!-- Keyboard help -->
<dialog id="help-dialog" class="dialog">
    <div class="dialog__form">
        <h3 class="dialog__title">Keyboard shortcuts</h3>
        <ul class="shortcut-list">
            <li><kbd>N</kbd> New device</li>
            <li><kbd>L</kbd> Learn button</li>
            <li><kbd>M</kbd> Add button manually</li>
            <li><kbd>E</kbd> Toggle edit mode</li>
            <li><kbd>S</kbd> Settings</li>
            <li><kbd>Del</kbd> Delete selected device / button</li>
            <li><kbd>↑</kbd> <kbd>↓</kbd> Navigate devices</li>
            <li><kbd>Arrows</kbd> Move focus between buttons</li>
            <li><kbd>Enter</kbd> Send focused button</li>
            <li><kbd>Esc</kbd> Close dialog</li>
            <li><kbd>?</kbd> This help</li>
        </ul>
        <div class="dialog__actions">
            <button type="button" class="btn btn--primary" data-close-dialog>Got it</button>
        </div>
    </div>
</dialog>
