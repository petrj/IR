<?php
/** Right panel: the virtual remote control (CSS Grid). */
?>
<section class="remote-pane" aria-label="Virtual remote">
    <div class="remote-pane__toolbar">
        <h2 id="remote-title">No device selected</h2>
        <div class="remote-pane__tools">
            <button type="button" class="btn btn--primary" data-action="learn" title="Learn a new button (L)">
                <span class="material-symbols-rounded">sensors</span><span class="btn__label">Learn</span>
            </button>
            <button type="button" class="btn btn--tonal" data-action="add-manual" title="Add a button by pasting its pulse sequence (M)">
                <span class="material-symbols-rounded">playlist_add</span><span class="btn__label">Add manually</span>
            </button>
            <button type="button" class="btn btn--tonal" data-action="toggle-edit" data-toggle="off" title="Toggle edit mode (E)">
                <span class="material-symbols-rounded">edit</span><span class="btn__label">Edit</span>
            </button>
            <button type="button" class="btn btn--tonal" data-action="delete-device" title="Delete device (Del)">
                <span class="material-symbols-rounded">delete</span><span class="btn__label">Delete</span>
            </button>
        </div>
    </div>

    <div class="remote-scroll">
        <div id="remote-grid" class="remote-grid" role="grid" aria-label="Remote buttons"></div>
    </div>

    <p id="remote-hint" class="remote-pane__hint">
        Select a device on the left, or press <kbd>L</kbd> to learn a new button.
    </p>
</section>
