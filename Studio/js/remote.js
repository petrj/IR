/**
 * Renders and manages the virtual remote (CSS Grid): buttons, empty cells,
 * placement mode, drag & drop, keyboard focus and occupancy checks.
 */

export class RemoteGrid {
    constructor(root, { rows, cols }, handlers = {}) {
        this.root = root;
        this.rows = rows;
        this.cols = cols;
        this.buttons = [];
        this.editing = false;
        this.selectedId = null;
        this.pendingPlacement = null; // {width, height, onPick}
        this.handlers = handlers;

        this.root.style.gridTemplateColumns = `repeat(${cols}, var(--cell-w))`;
        this.root.style.gridTemplateRows = `repeat(${rows}, var(--cell-h))`;
        this._applyCellSize();
        window.addEventListener('resize', () => this._applyCellSize());
    }

    _applyCellSize() {
        // Model a real TV remote (~20 cm long × ~12 cm wide). With a rows×cols
        // grid that means each cell is roughly four times as wide as it is tall.
        // We size the whole remote to fit the visible area on large screens; on
        // small screens it stays a usable minimum size and the container scrolls.
        const GAP = 8;   // keep in sync with .remote-grid gap
        const PAD = 14;  // keep in sync with .remote-grid padding
        const WIDTH_RATIO = 4; // cell width = WIDTH_RATIO × cell height
        const scroll = this.root.closest('.remote-scroll') || this.root.parentElement;

        const availW = scroll.clientWidth - 12;
        const availH = scroll.clientHeight - 12;

        // Height that lets the full remote fit vertically…
        let cellH = (availH - 2 * PAD - (this.rows - 1) * GAP) / this.rows;
        // …but never so wide it overflows horizontally.
        const cellHByWidth = ((availW - 2 * PAD - (this.cols - 1) * GAP) / this.cols) / WIDTH_RATIO;

        cellH = Math.min(cellH, cellHByWidth);
        cellH = Math.max(24, Math.min(cellH, 46));
        const cellW = Math.floor(cellH * WIDTH_RATIO);

        this.root.style.setProperty('--cell-h', `${Math.floor(cellH)}px`);
        this.root.style.setProperty('--cell-w', `${cellW}px`);
    }

    setButtons(buttons) {
        this.buttons = buttons;
        this.render();
    }

    setEditing(editing) {
        this.editing = editing;
        this.pendingPlacement = null;
        this.root.classList.toggle('is-editing', editing);
        this.render();
    }

    /** Enter placement mode for a button of the given size. */
    startPlacement(width, height, onPick) {
        this.pendingPlacement = { width, height, onPick };
        if (!this.editing) {
            this.root.classList.add('is-editing');
        }
        this.render();
    }

    cancelPlacement() {
        this.pendingPlacement = null;
        this.root.classList.toggle('is-editing', this.editing);
        this.render();
    }

    /** Build a Set of occupied "r,c" cell keys, optionally ignoring one button. */
    _occupancy(ignoreId = null) {
        const occ = new Set();
        for (const b of this.buttons) {
            if (b.id === ignoreId) continue;
            for (let r = 0; r < b.height; r++) {
                for (let c = 0; c < b.width; c++) {
                    occ.add(`${b.grid_row + r},${b.grid_column + c}`);
                }
            }
        }
        return occ;
    }

    /** Can a w×h button be placed at (row,col) without overflow/overlap? */
    canPlace(row, col, width, height, ignoreId = null) {
        if (row < 1 || col < 1) return false;
        if (row + height - 1 > this.rows || col + width - 1 > this.cols) return false;
        const occ = this._occupancy(ignoreId);
        for (let r = 0; r < height; r++) {
            for (let c = 0; c < width; c++) {
                if (occ.has(`${row + r},${col + c}`)) return false;
            }
        }
        return true;
    }

    render() {
        this.root.innerHTML = '';

        // Empty cells (shown in edit / placement mode for interaction & feedback).
        const occ = this._occupancy();
        for (let r = 1; r <= this.rows; r++) {
            for (let c = 1; c <= this.cols; c++) {
                if (occ.has(`${r},${c}`)) continue;
                const cell = document.createElement('div');
                cell.className = 'grid-cell';
                cell.style.gridArea = `${r} / ${c} / ${r + 1} / ${c + 1}`;
                cell.dataset.row = r;
                cell.dataset.col = c;

                if (this.pendingPlacement) {
                    const { width, height } = this.pendingPlacement;
                    if (this.canPlace(r, c, width, height)) {
                        cell.classList.add('is-placing');
                        cell.addEventListener('click', () => {
                            const pick = this.pendingPlacement.onPick;
                            this.pendingPlacement = null;
                            pick(r, c);
                        });
                    }
                } else if (this.editing) {
                    cell.addEventListener('click', () => this.handlers.onEmptyClick?.(r, c));
                }
                this.root.appendChild(cell);
            }
        }

        // Buttons
        for (const b of this.buttons) {
            this.root.appendChild(this._renderButton(b));
        }
    }

    _renderButton(b) {
        const el = document.createElement('button');
        el.type = 'button';
        el.className = `remote-btn shape-${b.shape}`;
        el.style.gridArea = `${b.grid_row} / ${b.grid_column} / ${b.grid_row + b.height} / ${b.grid_column + b.width}`;
        el.style.setProperty('--btn-color', b.color);
        el.dataset.id = b.id;
        el.tabIndex = 0;
        el.setAttribute('role', 'gridcell');
        el.title = b.name;

        if (b.id === this.selectedId) el.classList.add('is-selected');

        el.innerHTML = `<span class="remote-btn__name">${escapeHtml(b.name)}</span>
            <span class="remote-btn__edit"><span class="material-symbols-rounded">edit</span></span>`;

        el.addEventListener('click', (e) => {
            if (this.editing) {
                e.preventDefault();
                this.handlers.onButtonEdit?.(b);
            } else {
                this.handlers.onButtonActivate?.(b);
            }
        });

        // Drag & drop repositioning (edit mode only).
        el.draggable = false;
        el.addEventListener('pointerdown', (e) => {
            if (!this.editing) return;
            this._beginDrag(b, el, e);
        });

        return el;
    }

    _beginDrag(button, el, startEvent) {
        startEvent.preventDefault();
        let moved = false;
        const startX = startEvent.clientX;
        const startY = startEvent.clientY;

        const onMove = (e) => {
            if (!moved && Math.hypot(e.clientX - startX, e.clientY - startY) > 6) {
                moved = true;
                el.classList.add('is-dragging');
            }
        };

        const onUp = (e) => {
            document.removeEventListener('pointermove', onMove);
            document.removeEventListener('pointerup', onUp);
            el.classList.remove('is-dragging');
            if (!moved) return;

            const target = document.elementFromPoint(e.clientX, e.clientY);
            const cell = target?.closest('.grid-cell, .remote-btn');
            let row, col;
            if (cell?.classList.contains('grid-cell')) {
                row = +cell.dataset.row;
                col = +cell.dataset.col;
            } else if (cell?.classList.contains('remote-btn') && +cell.dataset.id !== button.id) {
                return; // dropped on another button — ignore
            } else {
                return;
            }
            if (this.canPlace(row, col, button.width, button.height, button.id)) {
                this.handlers.onButtonMove?.(button, row, col);
            } else {
                this.handlers.onInvalidMove?.();
            }
        };

        document.addEventListener('pointermove', onMove);
        document.addEventListener('pointerup', onUp);
    }

    select(id) {
        this.selectedId = id;
        this.root.querySelectorAll('.remote-btn').forEach((el) => {
            el.classList.toggle('is-selected', +el.dataset.id === id);
        });
    }

    fire(id) {
        const el = this.root.querySelector(`.remote-btn[data-id="${id}"]`);
        if (el) {
            el.classList.add('is-firing');
            el.addEventListener('animationend', () => el.classList.remove('is-firing'), { once: true });
        }
    }

    focusButton(id) {
        this.root.querySelector(`.remote-btn[data-id="${id}"]`)?.focus();
    }

    /** Return the button spatially nearest in a direction from the focused one. */
    neighbour(fromId, dir) {
        const from = this.buttons.find((b) => b.id === fromId);
        if (!from) return this.buttons[0] ?? null;
        const fr = from.grid_row + from.height / 2;
        const fc = from.grid_column + from.width / 2;

        let best = null;
        let bestScore = Infinity;
        for (const b of this.buttons) {
            if (b.id === fromId) continue;
            const br = b.grid_row + b.height / 2;
            const bc = b.grid_column + b.width / 2;
            const dr = br - fr;
            const dc = bc - fc;
            const ok =
                (dir === 'up' && dr < -0.1) ||
                (dir === 'down' && dr > 0.1) ||
                (dir === 'left' && dc < -0.1) ||
                (dir === 'right' && dc > 0.1);
            if (!ok) continue;
            const primary = dir === 'up' || dir === 'down' ? Math.abs(dr) : Math.abs(dc);
            const cross = dir === 'up' || dir === 'down' ? Math.abs(dc) : Math.abs(dr);
            const score = primary + cross * 3;
            if (score < bestScore) {
                bestScore = score;
                best = b;
            }
        }
        return best;
    }
}

function escapeHtml(str) {
    return String(str).replace(/[&<>"']/g, (c) =>
        ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c])
    );
}
