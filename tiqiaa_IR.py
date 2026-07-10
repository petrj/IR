#!/usr/bin/env python3

import usb.core
import usb.util
import time
import os
import sys
import atexit
import readline
import json
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

# Command history configuration (UP/DOWN arrows)
HISTORY_FILE = os.path.expanduser("~/.tiqiaa_ir_history")
try:
    if os.path.exists(HISTORY_FILE):
        readline.read_history_file(HISTORY_FILE)
    # Disable automatic history so that only signals are stored (not menu choices)
    readline.set_auto_history(False)
except Exception:
    pass

def save_history():
    try:
        readline.write_history_file(HISTORY_FILE)
    except Exception:
        pass

atexit.register(save_history)

VID = 0x10c4
PID = 0x8468

EP_OUT = 0x01
EP_IN = 0x81

INTERFACE = 0

# Default IR pulse sequence used when no input is provided (Sharp Repro EQ button)
DEFAULT_SEQUENCE = [
    6944, -4576, 544, -608, 544, -1728, 544, -608, 544, -608, 544, -608, 544,
    -608, 544, -608, 544, -608, 544, -1728, 544, -608, 544, -1728, 592, -1680,
    592, -1680, 592, -1680, 592, -560, 592, -1680, 544, -1728, 544, -608, 544,
    -1728, 544, -1728, 544, -608, 544, -1728, 544, -1728, 544, -608, 544, -608,
    544, -1728, 544, -608, 544, -608, 544, -1728, 544, -608, 544, -608, 544,
    -1728, 544, -40688, 9008, -2288, 592, -304
]


class TiqiaaTview:

    def __init__(self, default_frequency=38):
        self.default_frequency = default_frequency
        self.dev = usb.core.find(
            idVendor=VID,
            idProduct=PID
        )

        if self.dev is None:
            raise RuntimeError(
                "Tiqiaa Tview not found"
            )

        print("Device found")
        print(
            self.dev.manufacturer,
            self.dev.product
        )

        self.dev.set_configuration()

        if self.dev.is_kernel_driver_active(INTERFACE):
            print(
                "Detaching kernel driver"
            )
            self.dev.detach_kernel_driver(
                INTERFACE
            )

        usb.util.claim_interface(
            self.dev,
            INTERFACE
        )

        self.cmd_id = 1
        self.usb_pack_counter = 1
        self.testing = False
        print(
            "Interface OK"
        )

    def close(self):
        try:
            self.set_idle()
        except:
            pass
        try:
            usb.util.release_interface(
                self.dev,
                INTERFACE
            )
            usb.util.dispose_resources(
                self.dev
            )
        except:
            pass

    #
    # ST <id> <cmd> EN
    #
    def command(self, cmd, extra_bytes=None):
        # Command base: ST + id + cmd
        packet_header = bytearray([0x53, 0x54, self.cmd_id, ord(cmd)])

        if extra_bytes:
            packet_header.extend(extra_bytes)

        # Command terminator: EN
        packet_header.extend([0x45, 0x4e])

        self.send_bulk(bytes(packet_header))

        self.cmd_id += 1
        if self.cmd_id > 200:
            self.cmd_id = 1

    #
    # USB bulk packet (with fragmentation according to the protocol)
    #
    def send_bulk(self, data):
        # The maximum data size in a single fragment is 56 bytes.
        # If the data is longer, the protocol requires splitting (fragmentation).
        max_chunk = 56
        chunks = [data[i:i + max_chunk] for i in range(0, len(data), max_chunk)]
        total_chunks = len(chunks)

        for idx, chunk in enumerate(chunks):
            packet = bytearray(5 + len(chunk))
            packet[0] = 0x02              # Report ID
            packet[1] = len(chunk) + 3    # Fragment size + 3
            packet[2] = self.usb_pack_counter  # Packet counter (1..15)
            packet[3] = total_chunks      # Total number of fragments
            packet[4] = idx + 1           # Current fragment index (1-based)

            packet[5:] = chunk

            if self.testing or total_chunks > 1:
                print(f"[DEBUG] Writing fragment {idx+1}/{total_chunks} (length: {len(chunk)} B, counter: {packet[2]})...")

            self.dev.write(
                EP_OUT,
                packet,
                timeout=3000
            )

            # The firmware is slow, wait a bit between fragments/packets
            time.sleep(0.15)

        # Increment the counter for the next command and clamp it to 1..15
        self.usb_pack_counter += 1
        if self.usb_pack_counter > 15:
            self.usb_pack_counter = 1

    #
    # read the response
    #
    def read_data(self, length=1024):
        try:
            data = self.dev.read(
                EP_IN,
                length,
                timeout=1000
            )
            return bytes(data)
        except usb.core.USBTimeoutError:
            return None

    #
    # flush the input buffer
    #
    def flush_input(self):
        print("Flushing USB input buffer...")
        flushed_bytes = 0
        while True:
            try:
                data = self.dev.read(EP_IN, 1024, timeout=50)
                if not data:
                    break
                flushed_bytes += len(data)
            except usb.core.USBError:
                break
        if flushed_bytes > 0:
            print(f"Flushed {flushed_bytes} bytes of stale data.")

    #
    # read all fragments and assemble the payload
    #
    def read_payload(self):
        # Read the first fragment
        first_packet = self.read_data()
        if not first_packet:
            return None

        # Make sure the packet has a reasonable length (at least the 5-byte header)
        if len(first_packet) < 5:
            return first_packet

        report_id = first_packet[0]
        frag_len = first_packet[1] - 3  # Data length in this fragment
        total_chunks = first_packet[3]
        chunk_idx = first_packet[4]

        # If it is a HID report with multiple fragments
        if report_id in (0x01, 0x02) and total_chunks > 1:
            payload = bytearray(first_packet[5:5+frag_len])

            # Read the remaining fragments
            for idx in range(2, total_chunks + 1):
                next_packet = self.read_data()
                if next_packet and len(next_packet) >= 5:
                    next_len = next_packet[1] - 3
                    payload.extend(next_packet[5:5+next_len])
                else:
                    break
            return bytes(payload)
        else:
            # If it is only a single fragment, return its payload
            if report_id in (0x01, 0x02) and len(first_packet) >= 5:
                return bytes(first_packet[5:5+frag_len])
            return first_packet

    #
    # initialization
    #
    def init(self):
        self.flush_input()
        print("\n=== VERSION ===")
        self.command("V")
        time.sleep(0.5)
        res = self.read_data()
        if res:
            print("Firmware version (RAW):", res.hex())

    #
    # switch to standby / idle mode
    #
    def set_idle(self):
        self.command("L")
        time.sleep(0.5)
        self.read_data()

    #
    # switch to receive mode
    #
    def start_receive(self):
        print("\n=== RECEIVE MODE ACTIVATED ===")
        self.command("R")  # Switch the transceiver into Receive mode
        time.sleep(0.5)
        self.read_data()

    #
    # switch to transmit mode
    #
    def start_transmit(self):
        print("\n=== TRANSMIT MODE ACTIVATED ===")
        self.command("S")  # Switch the transceiver into Send mode
        time.sleep(0.5)
        self.read_data()

    #
    # Correct the short leading AGC header mark.
    #
    # The IR receiver's automatic gain control has not settled at the very start
    # of a transmission, so the FIRST burst is always measured shorter than it
    # really is (e.g. ~6944 us instead of the NEC-standard 9000 us). A too-short
    # header makes many devices reject the replayed code, so we snap a NEC-style
    # leader (short mark followed by a ~4500 us space) back to its nominal length.
    #
    @staticmethod
    def fix_leading_mark(pulses, verbose=False):
        NEC_LEADER_MARK_US = 9000
        if (len(pulses) >= 2
                and pulses[0] > 0
                and pulses[1] < 0
                and 3500 <= abs(pulses[1]) <= 5500   # NEC ~4500 us leader space
                and pulses[0] < 8000):               # measured shorter than it should be
            if verbose:
                print(f"[INFO] Correcting short leading mark {pulses[0]} us -> {NEC_LEADER_MARK_US} us")
            pulses[0] = NEC_LEADER_MARK_US
        return pulses

    #
    # send IR data (transmit)
    #
    def transmit_ir(self, pulse_array, frequency_khz=38):
        # Repair a short leading header that the receiver under-measured at
        # capture time, so previously stored codes also transmit reliably.
        pulse_array = self.fix_leading_mark(list(pulse_array))

        # Activate transmit mode right before sending the data
        self.start_transmit()

        print(f"Transmitting IR signal ({len(pulse_array)} pulses, frequency {frequency_khz}kHz)...")

        extra_bytes = bytearray()
        extra_bytes.append(frequency_khz)  # Add the frequency (e.g. 38 = 0x26)

        for p in pulse_array:
            state_bit = 0x80 if p > 0 else 0x00  # Positive number = ON (0x80), negative = OFF (0x00)
            duration_units = round(abs(p) / 16)    # Convert from us to 16us units

            if duration_units == 0:
                continue

            # If the pulse is longer than 127 units (approx. 2ms), it must be split
            # into multiple bytes carrying the same state bit.
            while duration_units > 127:
                extra_bytes.append(state_bit | 127)
                duration_units -= 127
            if duration_units > 0:
                extra_bytes.append(state_bit | duration_units)

        # Send the 'D' command with our converted data
        self.command("D", extra_bytes)

        # Read the acknowledgement from the device (we expect a response)
        time.sleep(0.5)
        res = self.read_data()
        if res:
            print(f"[DEBUG] Response to transmit (D): {res.hex()}")

        print("Transmission complete.")

        # Return the device to standby mode
        self.set_idle()

    #
    # request IR data (while in receive mode)
    #
    def request_ir(self):
        self.command("O")

    #
    # decode IR
    #
    def decode(self, payload, testing=False, print_result=True):
        if testing:
            print(f"\n[TESTING] Raw payload (length {len(payload)} B): {payload.hex()}")

        # Look for the 'D' marker
        try:
            pos = payload.index(ord('D'))
        except ValueError:
            if testing:
                print("[TESTING] No 'D' marker denoting IR data was found in the payload.")
            return None

        # Make sure the ST <id> header precedes 'D'
        if pos >= 3 and payload[pos-3] == 0x53 and payload[pos-2] == 0x54:
            cmd_id = payload[pos-1]
            if testing:
                print(f"[TESTING] Valid header found: ST, command ID: {cmd_id}")
        else:
            if testing:
                print("[TESTING] Warning: 'D' marker found, but the preceding 'ST <id>' header is missing")

        # The frequency byte comes right after 'D'
        freq_byte = payload[pos+1]
        if testing:
            print(f"[TESTING] Frequency byte: 0x{freq_byte:02X} ({freq_byte} kHz)")

        # IR data starts after the frequency byte
        ir_data_start = pos + 2

        # Look for the "EN" terminator (0x45, 0x4E) at the end of the payload
        try:
            end_pos = payload.index(b'EN', ir_data_start)
            ir_data = payload[ir_data_start:end_pos]
            if testing:
                print(f"[TESTING] Found 'EN' terminator at position {end_pos}. IR data length: {len(ir_data)} B")
        except ValueError:
            # If 'EN' happens to be missing (it should not), take the rest
            ir_data = payload[ir_data_start:]
            if testing:
                print(f"[TESTING] Warning: 'EN' terminator not found. Taking all remaining data ({len(ir_data)} B)")

        # 1. Merge consecutive bytes carrying the same state (ON/OFF)
        merged_pulses = []
        for idx, b in enumerate(ir_data):
            state_on = bool(b & 0x80)
            duration = (b & 0x7f) * 16

            if merged_pulses and ((merged_pulses[-1] > 0) == state_on):
                if state_on:
                    merged_pulses[-1] += duration
                else:
                    merged_pulses[-1] -= duration
                if testing:
                    print(f"  [{idx:02d}] Byte: 0x{b:02X} -> Merged with previous (State: {'ON' if state_on else 'OFF'}, Total duration: {abs(merged_pulses[-1])} us)")
            else:
                merged_pulses.append(duration if state_on else -duration)
                if testing:
                    print(f"  [{idx:02d}] Byte: 0x{b:02X} -> New segment (State: {'ON' if state_on else 'OFF'}, Duration: {duration} us)")

        # 2. Noise filtering (remove pulses that are too short < 50us)
        pulses = []
        noise_pulses = []
        for p in merged_pulses:
            if abs(p) < 50:
                noise_pulses.append(p)
            else:
                pulses.append(p)

        if testing:
            print(f"\n[TESTING] Statistics:")
            print(f"  Total raw segments after merge: {len(merged_pulses)}")
            print(f"  Number of valid pulses (> 50 us): {len(pulses)}")
            print(f"  Filtered-out noise (< 50 us): {noise_pulses}")

        if len(pulses) < 4:
            if testing:
                print("[TESTING] Too few valid pulses (< 4), ignoring the code.")
            return None

        # 3. Correct the short leading AGC header mark (see fix_leading_mark()).
        self.fix_leading_mark(pulses, verbose=testing or print_result)

        pulse_str = ", ".join(map(str, pulses))

        if print_result:
            print("\n" + "="*50)
            print(f" IR CODE CAPTURED (Pulse count: {len(pulses)})")
            print("="*50)
            print("Format for re-transmission:")
            print(f"[{pulse_str}]")
            print("="*50)

        # Add the captured code to the command history (available via arrow up)
        try:
            readline.add_history(pulse_str)
        except Exception:
            pass

        return pulses

    #
    # capture a single IR code (used by the REST API)
    #
    def capture_once(self, timeout=10.0, testing=False):
        self.start_receive()
        deadline = time.time() + timeout
        try:
            while time.time() < deadline:
                try:
                    self.request_ir()
                    time.sleep(0.05)
                    payload = self.read_payload()
                    if payload:
                        pulses = self.decode(payload, testing=testing, print_result=False)
                        if pulses:
                            return pulses
                    time.sleep(0.25)
                except usb.core.USBError as e:
                    print("USB ERROR:", e)
                    time.sleep(1)
        finally:
            self.set_idle()
        return None

    #
    # main learning loop
    #
    def learn(self, testing=False):
        self.start_receive()
        if testing:
            print("\n=== WAITING FOR A BUTTON PRESS (TESTING MODE) ===")
        else:
            print("\n=== WAITING FOR A BUTTON PRESS ===")
        print("Press Ctrl+C to return to the main menu.")

        while True:
            try:
                self.request_ir()
                time.sleep(0.05)
                payload = self.read_payload()

                if payload:
                    self.decode(payload, testing=testing)

                time.sleep(0.25)

            except usb.core.USBError as e:
                print("USB ERROR:", e)
                time.sleep(1)

    #
    # detailed dump of USB device information (for Testing mode)
    #
    def print_detailed_hw_info(self):
        print("\n" + "="*50)
        print(" DETAILED USB HARDWARE INFORMATION")
        print("="*50)

        dev = self.dev
        print(f"Manufacturer:      {dev.manufacturer}")
        print(f"Product:           {dev.product}")
        print(f"Serial number:     {dev.serial_number}")
        print(f"Vendor ID (VID):   0x{dev.idVendor:04X} ({dev.idVendor})")
        print(f"Product ID (PID):  0x{dev.idProduct:04X} ({dev.idProduct})")
        print(f"Device class:      {dev.bDeviceClass}")
        print(f"Subclass:          {dev.bDeviceSubClass}")
        print(f"Protocol:          {dev.bDeviceProtocol}")
        print(f"Max EP0 size:      {dev.bMaxPacketSize0}")
        print(f"Configurations:    {dev.bNumConfigurations}")

        for cfg in dev:
            print(f"\nConfiguration {cfg.bConfigurationValue}:")
            print(f"  Description (index): {cfg.iConfiguration}")
            print(f"  Total length:        {cfg.wTotalLength}")
            print(f"  Number of interfaces: {cfg.bNumInterfaces}")

            for intf in cfg:
                print(f"  Interface {intf.bInterfaceNumber} (Alternate setting {intf.bAlternateSetting}):")
                print(f"    Interface class: {intf.bInterfaceClass}")
                print(f"    Subclass:        {intf.bInterfaceSubClass}")
                print(f"    Protocol:        {intf.bInterfaceProtocol}")
                print(f"    Endpoints:       {intf.bNumEndpoints}")

                for ep in intf:
                    print(f"    Endpoint 0x{ep.bEndpointAddress:02X}:")
                    print(f"      Direction:    {'Input (IN)' if usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_IN else 'Output (OUT)'}")
                    print(f"      Transfer type: {usb.util.endpoint_type(ep.bmAttributes)}")
                    print(f"      Max packet size: {ep.wMaxPacketSize}")
                    print(f"      Polling interval: {ep.bInterval}")
        print("="*50 + "\n")

    #
    # sending mode loop (Sending mode)
    #
    def sending_mode(self):
        print("\n=== TRANSMIT MODE (Sending mode) ===")
        print("Enter a pulse sequence as numbers separated by commas or spaces (e.g. 256, -672, 192).")
        print("Positive values are pulses (transmitter on), negative values are gaps (transmitter off).")
        print("Press Ctrl+C to return to the main menu, or type 'q' / 'exit'.")

        while True:
            try:
                user_input = input("\nSequence to send [Enter for default]: ").strip()
                if user_input.lower() in ('q', 'exit'):
                    print("Leaving transmit mode...")
                    break

                if not user_input:
                    pulses = DEFAULT_SEQUENCE
                    print(f"Using the default sequence ({len(pulses)} pulses)...")
                else:
                    try:
                        pulses = self.parse_pulse_sequence(user_input)
                        if not pulses:
                            print("The sequence is empty, please try again.")
                            continue
                        # Add the successfully parsed sequence to the history
                        readline.add_history(user_input)
                    except ValueError as ve:
                        print(f"Error while parsing the sequence: {ve}")
                        continue

                self.transmit_ir(pulses, self.default_frequency)

            except KeyboardInterrupt:
                # Will be caught in the main block to return to the menu
                raise

    #
    # helper method to parse a pulse sequence from a string
    #
    def parse_pulse_sequence(self, input_str):
        cleaned = input_str.strip()
        if cleaned.startswith('[') and cleaned.endswith(']'):
            cleaned = cleaned[1:-1]

        cleaned = cleaned.replace(',', ' ')
        parts = cleaned.split()

        pulses = []
        for part in parts:
            part = part.strip()
            if not part:
                continue
            try:
                pulses.append(int(part))
            except ValueError:
                raise ValueError(f"Invalid number: '{part}'")

        return pulses


#
# ===========================================================================
# REST API server
# ===========================================================================
#
# The USB device can only be used by a single operation at a time, therefore
# every access is serialized through a global lock.
#

class IRRequestHandler(BaseHTTPRequestHandler):
    device = None
    lock = None
    # Reject request bodies larger than this (basic protection against abuse)
    MAX_BODY_SIZE = 1 * 1024 * 1024

    def log_message(self, fmt, *args):
        # Route the access log through the standard print for consistency
        print("[API] %s - %s" % (self.address_string(), fmt % args))

    def _send_json(self, code, obj):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _read_json_body(self):
        length = int(self.headers.get("Content-Length", 0))
        if length <= 0:
            return {}
        if length > self.MAX_BODY_SIZE:
            raise ValueError("Request body too large")
        raw = self.rfile.read(length)
        if not raw:
            return {}
        return json.loads(raw.decode("utf-8"))

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/")
        query = parse_qs(parsed.query)

        if path in ("", "/api", "/api/help"):
            self._send_json(200, {
                "service": "Tiqiaa Tview IR REST API",
                "endpoints": {
                    "GET /api/status": "Device information",
                    "GET /api/read?timeout=10": "Capture a single IR sequence",
                    "POST /api/send": "Transmit an IR sequence. Body: {\"pulses\": [...], \"frequency\": 38}"
                }
            })
            return

        if path == "/api/status":
            with self.lock:
                dev = self.device.dev
                self._send_json(200, {
                    "manufacturer": dev.manufacturer,
                    "product": dev.product,
                    "serial_number": dev.serial_number,
                    "vendor_id": f"0x{dev.idVendor:04X}",
                    "product_id": f"0x{dev.idProduct:04X}",
                    "default_frequency": self.device.default_frequency
                })
            return

        if path == "/api/read":
            try:
                timeout = float(query.get("timeout", ["10"])[0])
            except (ValueError, IndexError):
                self._send_json(400, {"error": "Invalid 'timeout' parameter"})
                return
            with self.lock:
                pulses = self.device.capture_once(timeout=timeout)
            if pulses:
                self._send_json(200, {"pulses": pulses, "count": len(pulses)})
            else:
                self._send_json(408, {"error": "No IR code captured within the timeout"})
            return

        self._send_json(404, {"error": "Unknown endpoint"})

    def do_POST(self):
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/")

        if path == "/api/send":
            try:
                data = self._read_json_body()
            except ValueError as e:
                self._send_json(400, {"error": f"Invalid request body: {e}"})
                return
            except json.JSONDecodeError:
                self._send_json(400, {"error": "Invalid JSON"})
                return

            pulses = data.get("pulses")
            if not isinstance(pulses, list) or not pulses:
                self._send_json(400, {"error": "'pulses' must be a non-empty array of integers"})
                return
            try:
                pulses = [int(p) for p in pulses]
            except (ValueError, TypeError):
                self._send_json(400, {"error": "'pulses' must contain integers only"})
                return

            frequency = data.get("frequency", self.device.default_frequency)
            try:
                frequency = int(frequency)
            except (ValueError, TypeError):
                self._send_json(400, {"error": "'frequency' must be an integer"})
                return

            with self.lock:
                try:
                    self.device.transmit_ir(pulses, frequency)
                except usb.core.USBError as e:
                    self._send_json(500, {"error": f"USB error: {e}"})
                    return
            self._send_json(200, {"status": "sent", "count": len(pulses), "frequency": frequency})
            return

        self._send_json(404, {"error": "Unknown endpoint"})


def run_api_server(device, host, port):
    IRRequestHandler.device = device
    IRRequestHandler.lock = threading.Lock()
    server = ThreadingHTTPServer((host, port), IRRequestHandler)
    print(f"\n=== REST API SERVER RUNNING ON http://{host}:{port} ===")
    print("Available endpoints:")
    print("  GET  /api/status")
    print("  GET  /api/read?timeout=10")
    print("  POST /api/send   Body: {\"pulses\": [...], \"frequency\": 38}")
    print("Press Ctrl+C to stop the server.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping the API server...")
    finally:
        # serve_forever() has already returned here, so only close the socket.
        server.server_close()


def run_menu(dev, host="127.0.0.1", port=8080):
    while True:
        print("\n=== MAIN MENU ===")
        print(" R) Reading mode")
        print(" S) Sending mode")
        print(" T) Testing mode")
        print(" W) Web server (REST API)")
        print(" Q) Quit")

        try:
            choice = input("Select an option (R/S/T/W/Q): ").strip().upper()
        except (KeyboardInterrupt, EOFError):
            print("\nExiting the program...")
            break

        if choice in ("R", "READ", "READING"):
            try:
                dev.learn(testing=False)
            except KeyboardInterrupt:
                print("\nReturning to the main menu...")
                dev.set_idle()
        elif choice in ("S", "SEND", "SENDING"):
            try:
                dev.sending_mode()
            except KeyboardInterrupt:
                print("\nReturning to the main menu...")
                dev.set_idle()
        elif choice in ("T", "TEST", "TESTING"):
            try:
                dev.print_detailed_hw_info()
                dev.learn(testing=True)
            except KeyboardInterrupt:
                print("\nReturning to the main menu...")
                dev.set_idle()
        elif choice in ("W", "WEB", "API", "SERVER"):
            # The server blocks until stopped with Ctrl+C, then we return to the menu
            run_api_server(dev, host, port)
            dev.set_idle()
        elif choice in ("Q", "QUIT", "EXIT"):
            print("Exiting the program...")
            break
        else:
            print("Invalid choice, please try again.")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Tiqiaa Tview USB IR transceiver control")
    parser.add_argument(
        "-f", "--frequency",
        type=int,
        default=255,
        help="Default transmit frequency in kHz (e.g. 255)"
    )
    parser.add_argument(
        "-m", "--mode",
        choices=["reading", "sending", "testing"],
        help="Run the given mode directly without showing the menu"
    )
    parser.add_argument(
        "--send",
        metavar="SEQUENCE",
        help="Transmit a pulse sequence once and exit (e.g. \"9000, -4500, 560, -560\")"
    )
    parser.add_argument(
        "--api",
        action="store_true",
        help="Start the REST API server instead of the interactive menu"
    )
    parser.add_argument(
        "--host",
        default="127.0.0.1",
        help="Host/interface for the REST API server (default: 127.0.0.1)"
    )
    parser.add_argument(
        "--port",
        type=int,
        default=8080,
        help="Port for the REST API server (default: 8080)"
    )
    args = parser.parse_args()

    dev = None
    try:
        dev = TiqiaaTview(default_frequency=args.frequency)
        dev.init()

        if args.api:
            run_api_server(dev, args.host, args.port)
        elif args.send is not None:
            pulses = dev.parse_pulse_sequence(args.send)
            if not pulses:
                print("The provided sequence is empty.")
            else:
                dev.transmit_ir(pulses, dev.default_frequency)
        elif args.mode == "reading":
            try:
                dev.learn(testing=False)
            except KeyboardInterrupt:
                print("\nExiting...")
                dev.set_idle()
        elif args.mode == "sending":
            try:
                dev.sending_mode()
            except KeyboardInterrupt:
                print("\nExiting...")
                dev.set_idle()
        elif args.mode == "testing":
            try:
                dev.print_detailed_hw_info()
                dev.learn(testing=True)
            except KeyboardInterrupt:
                print("\nExiting...")
                dev.set_idle()
        else:
            run_menu(dev, args.host, args.port)

    except RuntimeError as re:
        print("\nERROR:", re)
    except ValueError as ve:
        print("\nERROR:", ve)
    except KeyboardInterrupt:
        print("\nDone")
    finally:
        if dev:
            dev.close()
