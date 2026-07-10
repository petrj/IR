<?php
/** Left panel: the list of devices (virtual remotes). */
?>
<aside class="sidebar" aria-label="Devices">
    <div class="sidebar__head">
        <h2>Devices</h2>
        <button type="button" class="btn btn--primary btn--small" data-action="new-device" title="New device (N)">
            <span class="material-symbols-rounded">add</span><span class="btn__label">New device</span>
        </button>
    </div>
    <ul id="device-list" class="device-list" role="listbox" aria-label="Device list" tabindex="0"></ul>
    <p id="device-empty" class="sidebar__empty" hidden>No devices yet. Create one to get started.</p>
</aside>
