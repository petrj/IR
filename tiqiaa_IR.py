#!/usr/bin/env python3

import usb.core
import usb.util
import time

VID = 0x10c4
PID = 0x8468

EP_OUT = 0x01
EP_IN = 0x81

INTERFACE = 0


class TiqiaaTview:

    def __init__(self):
        self.dev = usb.core.find(
            idVendor=VID,
            idProduct=PID
        )

        if self.dev is None:
            raise RuntimeError(
                "Tiqiaa Tview nenalezen"
            )

        print("Zařízení nalezeno")
        print(
            self.dev.manufacturer,
            self.dev.product
        )

        self.dev.set_configuration()

        if self.dev.is_kernel_driver_active(INTERFACE):
            print(
                "Odpojuji kernel driver"
            )
            self.dev.detach_kernel_driver(
                INTERFACE
            )

        usb.util.claim_interface(
            self.dev,
            INTERFACE
        )

        self.cmd_id = 1
        print(
            "Interface OK"
        )

    def close(self):
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
        # Základ příkazu: ST + id + cmd
        packet_header = bytearray([0x53, 0x54, self.cmd_id, ord(cmd)])
        
        if extra_bytes:
            packet_header.extend(extra_bytes)
            
        # Konec příkazu: EN
        packet_header.extend([0x45, 0x4e])

        self.send_bulk(bytes(packet_header))

        self.cmd_id += 1
        if self.cmd_id > 200:
            self.cmd_id = 1

    #
    # USB bulk packet (s fragmentací podle protokolu)
    #
    def send_bulk(self, data):
        # Maximální velikost dat v jednom fragmentu je 56 bajtů
        # Pokud by data byla delší, protokol vyžaduje rozdělení (fragmentaci)
        max_chunk = 56
        chunks = [data[i:i + max_chunk] for i in range(0, len(data), max_chunk)]
        total_chunks = len(chunks)

        for idx, chunk in enumerate(chunks):
            packet = bytearray(64)
            packet[0] = 0x02              # Report ID
            packet[1] = len(chunk) + 3    # Velikost fragmentu + 3
            packet[2] = 1                 # Counter paketů
            packet[3] = total_chunks      # Celkový počet fragmentů
            packet[4] = idx + 1           # Index aktuálního fragmentu (1-based)

            packet[5:5+len(chunk)] = chunk

            self.dev.write(
                EP_OUT,
                packet,
                timeout=3000
            )
            
            # Firmware je pomalý, chvilku počkáme mezi fragmenty/pakety
            time.sleep(0.15)

    #
    # přečtení odpovědi
    #
    def read_data(self):
        try:
            data = self.dev.read(
                EP_IN,
                64,
                timeout=1000
            )
            return bytes(data)
        except usb.core.USBTimeoutError:
            return None

    #
    # inicializace
    #
    def init(self):
        print("\n=== VERSION ===")
        self.command("V")
        time.sleep(0.5)
        res = self.read_data()
        if res:
            print("Verze firmware (RAW):", res.hex())

    #
    # přepnutí do režimu příjmu
    #
    def start_receive(self):
        print("\n=== REŽIM PŘÍJMU AKTIVOVÁN ===")
        self.command("R")  # Přepnutí transceiveru do Receive modu
        time.sleep(0.5)

    #
    # přepnutí do režimu vysílání
    #
    def start_transmit(self):
        print("\n=== REŽIM VYSÍLÁNÍ AKTIVOVÁN ===")
        self.command("S")  # Přepnutí transceiveru do Send modu
        time.sleep(0.5)

    #
    # odeslání IR dat (vysílání)
    #
    def transmit_ir(self, pulse_array, frequency_khz=38):
        print(f"Vysílám IR signál ({len(pulse_array)} pulzů, frekvence {frequency_khz}kHz)...")
        
        extra_bytes = bytearray()
        extra_bytes.append(frequency_khz) # Přidání frekvence (např. 38 = 0x26)
        
        for p in pulse_array:
            state_bit = 0x80 if p > 0 else 0x00  # Kladné číslo = ON (0x80), záporné = OFF (0x00)
            duration_units = min(int(abs(p) / 16), 0x7F) # Převod z us zpět na jednotky 16us (max 127)
            
            extra_bytes.append(state_bit | duration_units)
            
        # Odešleme příkaz 'D' s našimi zkonvertovanými daty
        self.command("D", extra_bytes)
        time.sleep(0.5)
        print("Vysílání dokončeno.")

    #
    # požadavek na IR data (v režimu příjmu)
    #
    def request_ir(self):
        self.command("O")

    #
    # dekódování IR (zlidštěná verze)
    #
    def decode(self, data):
        try:
            pos = data.index(ord('D'))
        except ValueError:
            return

        ir = data[pos+1:]
        pulses = []

        for b in ir:
            if b in (0, 0xff, 0x7f):
                continue

            state_on = bool(b & 0x80)
            duration = (b & 0x7f) * 16

            if duration < 50: # filtrace šumu
                continue

            pulses.append(duration if state_on else -duration)

        if len(pulses) < 4:
            return

        print("\n" + "="*50)
        print(f" ZACHYCEN IR KÓD (Počet pulzů: {len(pulses)})")
        print("="*50)
        print("Formát pro zpětné vysílání:")
        print(f"[{', '.join(map(str, pulses))}]")
        print("="*50)

    #
    # hlavní smyčka pro učení
    #
    def learn(self):
        self.start_receive()
        print("\n=== ČEKÁM NA STISK TLAČÍTKA ===")

        while True:
            try:
                self.request_ir()
                time.sleep(0.05)
                data = self.read_data()

                if data:
                    self.decode(data)

                time.sleep(0.25)

            except usb.core.USBError as e:
                print("USB ERROR:", e)
                time.sleep(1)


if __name__ == "__main__":
    dev = None
    try:
        dev = TiqiaaTview()
        dev.init()

        # --- REŽIM 1: UČENÍ (ZÁKLADNÍ) ---
        dev.learn()

        # --- REŽIM 2: UKÁZKA VYSÍLÁNÍ ---
        # Pokud chceš kód vyslat, zakomentuj řádek `dev.learn()` výše,
        # odkomentuj řádky níže a vlož tam pole, které ti vypsal přijímač.
        
        # dev.start_transmit()
        # mojedata = [256, -672, 192, -304, -1104, -1248, 576, -560, 576, -1696, 576, -560, 576, -560, 576, -1696, 544, -528, 960, -224, 576, -304, -1104, -1248, -560, 544, -1712] # doplň svoje data
        # dev.transmit_ir(mojedata)

    except KeyboardInterrupt:
        print("\nKonec")

    finally:
        if dev:
            dev.close()