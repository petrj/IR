<?php
/**
 * Application bootstrap: loads configuration and registers a simple PSR-4-ish
 * autoloader for the App\ namespace under src/.
 */

$config = require __DIR__ . '/config.php';

spl_autoload_register(static function (string $class): void {
    $prefix = 'App\\';
    if (strncmp($class, $prefix, strlen($prefix)) !== 0) {
        return;
    }
    $relative = substr($class, strlen($prefix));
    $file = __DIR__ . '/src/' . str_replace('\\', '/', $relative) . '.php';
    if (file_exists($file)) {
        require $file;
    }
});

return $config;
