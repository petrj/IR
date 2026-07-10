<?php
/**
 * Main GUI entry point for the IR remote editor.
 *
 * This file only assembles the HTML shell from templates. All behaviour lives
 * in the JavaScript modules under js/, and all data access goes through api/.
 */

declare(strict_types=1);

ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);

$config = require __DIR__ . '/bootstrap.php';
?>
<!DOCTYPE html>
<html lang="en" data-theme="dark">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0">
    <meta name="color-scheme" content="dark">
    <title>IR Remote Studio</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@400;500;700&family=Material+Symbols+Rounded:opsz,wght,FILL,GRAD@24,400,0,0" rel="stylesheet">
    <link rel="stylesheet" href="css/style.css">
</head>
<body>
    <?php require __DIR__ . '/templates/header.php'; ?>

    <main class="layout">
        <?php require __DIR__ . '/templates/sidebar.php'; ?>
        <?php require __DIR__ . '/templates/remote.php'; ?>
    </main>

    <?php require __DIR__ . '/templates/dialogs.php'; ?>

    <div id="toast-host" class="toast-host" aria-live="polite"></div>

    <script>
        // Configuration injected from PHP so JS never hard-codes URLs/dimensions.
        window.APP_CONFIG = <?= json_encode([
            'apiBase'       => 'api/index.php',
            'gridRows'      => (int) $config['grid_rows'],
            'gridColumns'   => (int) $config['grid_columns'],
            'learnTimeout'  => (int) $config['learn_timeout'],
            'sendFrequency' => (int) $config['send_frequency'],
        ], JSON_UNESCAPED_SLASHES) ?>;
    </script>
    <script type="module" src="js/app.js"></script>
</body>
</html>
