<?php

namespace App;

/**
 * HTTP client for the Python IR REST backend (tiqiaa_IR.py --api).
 *
 * Only two operations are needed by the GUI:
 *   - read():  wait for and capture a single IR sequence (Learn mode)
 *   - send():  transmit a pulse sequence to one or more transmitters
 */
class IrClient
{
    public function __construct(private array $config)
    {
    }

    /**
     * Wait for an IR signal on the default backend and return its pulses.
     *
     * @return array{ok:bool, pulses?:array, count?:int, error?:string}
     */
    public function read(?int $timeout = null): array
    {
        $timeout = $timeout ?? (int) $this->config['learn_timeout'];
        $base = rtrim($this->config['ir_backend_url'], '/');
        $url = $base . $this->config['ir_read_path'] . '?timeout=' . $timeout;

        $result = $this->request('GET', $url, null, $timeout + 5);
        if (!$result['ok']) {
            return $result;
        }

        $body = $result['body'];
        if (!isset($body['pulses']) || !is_array($body['pulses']) || count($body['pulses']) === 0) {
            return ['ok' => false, 'error' => $body['error'] ?? 'No IR signal received (timeout).'];
        }

        return ['ok' => true, 'pulses' => $body['pulses'], 'count' => count($body['pulses'])];
    }

    /**
     * Transmit a pulse sequence to the given transmitter base URLs.
     * When no URLs are supplied the default backend is used.
     *
     * @return array{ok:bool, results:array, error?:string}
     */
    public function send(array $pulses, array $baseUrls = [], ?int $frequency = null): array
    {
        $frequency = $frequency ?? (int) $this->config['send_frequency'];
        if (!$baseUrls) {
            $baseUrls = [$this->config['ir_backend_url']];
        }

        $payload = ['pulses' => array_values(array_map('intval', $pulses)), 'frequency' => $frequency];

        $results = [];
        $anyOk = false;
        foreach ($baseUrls as $base) {
            $url = rtrim($base, '/') . $this->config['ir_send_path'];
            $res = $this->request('POST', $url, $payload, (int) $this->config['http_timeout']);
            $results[] = [
                'url'   => $base,
                'ok'    => $res['ok'],
                'error' => $res['ok'] ? null : ($res['error'] ?? 'Send failed'),
            ];
            $anyOk = $anyOk || $res['ok'];
        }

        return ['ok' => $anyOk, 'results' => $results];
    }

    /** Query the default backend for device status. */
    public function status(): array
    {
        $url = rtrim($this->config['ir_backend_url'], '/') . $this->config['ir_status_path'];
        return $this->request('GET', $url, null, (int) $this->config['http_timeout']);
    }

    /**
     * Perform an HTTP request and decode the JSON response.
     * Uses cURL when available, otherwise falls back to PHP streams.
     *
     * @return array{ok:bool, body?:array, error?:string, status?:int}
     */
    private function request(string $method, string $url, ?array $json, int $timeout): array
    {
        return function_exists('curl_init')
            ? $this->requestCurl($method, $url, $json, $timeout)
            : $this->requestStream($method, $url, $json, $timeout);
    }

    private function requestCurl(string $method, string $url, ?array $json, int $timeout): array
    {
        $ch = curl_init($url);
        $headers = ['Accept: application/json'];

        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($ch, CURLOPT_CUSTOMREQUEST, $method);
        curl_setopt($ch, CURLOPT_TIMEOUT, $timeout);
        curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 5);

        if ($json !== null) {
            $headers[] = 'Content-Type: application/json';
            curl_setopt($ch, CURLOPT_POSTFIELDS, json_encode($json));
        }
        curl_setopt($ch, CURLOPT_HTTPHEADER, $headers);

        $raw = curl_exec($ch);
        if ($raw === false) {
            $err = curl_error($ch);
            curl_close($ch);
            return ['ok' => false, 'error' => 'Cannot reach IR backend: ' . $err];
        }

        $status = (int) curl_getinfo($ch, CURLINFO_HTTP_CODE);
        curl_close($ch);

        return $this->decodeResponse($raw, $status);
    }

    private function requestStream(string $method, string $url, ?array $json, int $timeout): array
    {
        $headers = "Accept: application/json\r\n";
        $content = null;
        if ($json !== null) {
            $content = json_encode($json);
            $headers .= "Content-Type: application/json\r\n";
        }

        $context = stream_context_create(['http' => [
            'method'        => $method,
            'header'        => $headers,
            'content'       => $content,
            'timeout'       => $timeout,
            'ignore_errors' => true,
        ]]);

        $raw = @file_get_contents($url, false, $context);
        if ($raw === false) {
            $err = error_get_last()['message'] ?? 'connection error';
            return ['ok' => false, 'error' => 'Cannot reach IR backend: ' . $err];
        }

        // Extract status code from $http_response_header (set by the stream wrapper).
        $status = 0;
        if (isset($http_response_header[0]) && preg_match('#HTTP/\S+\s+(\d+)#', $http_response_header[0], $m)) {
            $status = (int) $m[1];
        }

        return $this->decodeResponse($raw, $status);
    }

    private function decodeResponse(string $raw, int $status): array
    {
        $body = json_decode($raw, true);
        if (!is_array($body)) {
            $body = ['raw' => $raw];
        }

        if ($status < 200 || $status >= 300) {
            return ['ok' => false, 'status' => $status, 'error' => $body['error'] ?? ('HTTP ' . $status), 'body' => $body];
        }

        return ['ok' => true, 'status' => $status, 'body' => $body];
    }
}
