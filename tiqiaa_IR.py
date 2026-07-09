#!/usr/bin/env python3

import usb.core
import usb.util
import time
import os
import atexit
import readline

# Konfigurace historie příkazů (UP/DOWN šipky)
HISTORY_FILE = os.path.expanduser("~/.tiqiaa_ir_history")
try:
    if os.path.exists(HISTORY_FILE):
        readline.read_history_file(HISTORY_FILE)
    # Vypneme automatické ukládání, abychom do historie přidávali pouze signály (ne volby v menu)
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


class TiqiaaTview:

    def __init__(self, default_frequency=38):
        self.default_frequency = default_frequency
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
            packet = bytearray(5 + len(chunk))
            packet[0] = 0x02              # Report ID
            packet[1] = len(chunk) + 3    # Velikost fragmentu + 3
            packet[2] = self.usb_pack_counter # Čítač paketů (1..15)
            packet[3] = total_chunks      # Celkový počet fragmentů
            packet[4] = idx + 1           # Index aktuálního fragmentu (1-based)

            packet[5:] = chunk

            if self.testing or total_chunks > 1:
                print(f"[DEBUG] Zapisuji fragment {idx+1}/{total_chunks} (délka: {len(chunk)} B, čítač: {packet[2]})...")

            self.dev.write(
                EP_OUT,
                packet,
                timeout=3000
            )
            
            # Firmware je pomalý, chvilku počkáme mezi fragmenty/pakety
            time.sleep(0.15)

        # Inkrementujeme čítač pro příští příkaz a omezíme na 1..15
        self.usb_pack_counter += 1
        if self.usb_pack_counter > 15:
            self.usb_pack_counter = 1

    #
    # přečtení odpovědi
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
    # vyčištění vstupního bufferu
    #
    def flush_input(self):
        print("Čistím vstupní USB buffer...")
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
            print(f"Vyčištěno {flushed_bytes} bajtů starých dat.")

    #
    # přečtení všech fragmentů a sestavení payloadu
    #
    def read_payload(self):
        # Přečte první fragment
        first_packet = self.read_data()
        if not first_packet:
            return None
            
        # Zkontrolujeme, zda má paket rozumnou délku (aspoň hlavičku 5 bajtů)
        if len(first_packet) < 5:
            return first_packet
            
        report_id = first_packet[0]
        frag_len = first_packet[1] - 3 # Délka dat v tomto fragmentu
        total_chunks = first_packet[3]
        chunk_idx = first_packet[4]
        
        # Pokud je to HID report a má více fragmentů
        if report_id in (0x01, 0x02) and total_chunks > 1:
            payload = bytearray(first_packet[5:5+frag_len])
            
            # Čteme další fragmenty
            for idx in range(2, total_chunks + 1):
                next_packet = self.read_data()
                if next_packet and len(next_packet) >= 5:
                    next_len = next_packet[1] - 3
                    payload.extend(next_packet[5:5+next_len])
                else:
                    break
            return bytes(payload)
        else:
            # Pokud je to jen jeden fragment, vrátíme jeho payload
            if report_id in (0x01, 0x02) and len(first_packet) >= 5:
                return bytes(first_packet[5:5+frag_len])
            return first_packet

    #
    # inicializace
    #
    def init(self):
        self.flush_input()
        print("\n=== VERSION ===")
        self.command("V")
        time.sleep(0.5)
        res = self.read_data()
        if res:
            print("Verze firmware (RAW):", res.hex())

    #
    # přepnutí do standby / idle režimu
    #
    def set_idle(self):
        self.command("L")
        time.sleep(0.5)
        self.read_data()

    #
    # přepnutí do režimu příjmu
    #
    def start_receive(self):
        print("\n=== REŽIM PŘÍJMU AKTIVOVÁN ===")
        self.command("R")  # Přepnutí transceiveru do Receive modu
        time.sleep(0.5)
        self.read_data()

    #
    # přepnutí do režimu vysílání
    #
    def start_transmit(self):
        print("\n=== REŽIM VYSÍLÁNÍ AKTIVOVÁN ===")
        self.command("S")  # Přepnutí transceiveru do Send modu
        time.sleep(0.5)
        self.read_data()

    #
    # odeslání IR dat (vysílání)
    #
    def transmit_ir(self, pulse_array, frequency_khz=38):
        # Aktivujeme vysílací režim těsně před posláním dat
        self.start_transmit()
        
        print(f"Vysílám IR signál ({len(pulse_array)} pulzů, frekvence {frequency_khz}kHz)...")
        
        extra_bytes = bytearray()
        extra_bytes.append(frequency_khz) # Přidání frekvence (např. 38 = 0x26)
        
        for p in pulse_array:
            state_bit = 0x80 if p > 0 else 0x00  # Kladné číslo = ON (0x80), záporné = OFF (0x00)
            duration_units = round(abs(p) / 16)    # Převod z us na jednotky 16us
            
            if duration_units == 0:
                continue
                
            # Pokud je pulz delší než 127 jednotek (cca 2ms), musíme ho rozdělit
            # do více bajtů se stejným stavovým bitem.
            while duration_units > 127:
                extra_bytes.append(state_bit | 127)
                duration_units -= 127
            if duration_units > 0:
                extra_bytes.append(state_bit | duration_units)
            
        # Odešleme příkaz 'D' s našimi zkonvertovanými daty
        self.command("D", extra_bytes)
        
        # Přečteme potvrzení ze zařízení (očekáváme odpověď)
        time.sleep(0.5)
        res = self.read_data()
        if res:
            print(f"[DEBUG] Odpověď na odeslání (D): {res.hex()}")
            
        print("Vysílání dokončeno.")
        
        # Vrátíme zařízení do standby režimu
        self.set_idle()

    #
    # požadavek na IR data (v režimu příjmu)
    #
    def request_ir(self):
        self.command("O")

    #
    # dekódování IR
    #
    def decode(self, payload, testing=False):
        if testing:
            print(f"\n[TESTING] Surový payload (délka {len(payload)} B): {payload.hex()}")

        # Hledáme marker 'D'
        try:
            pos = payload.index(ord('D'))
        except ValueError:
            if testing:
                print("[TESTING] V payloadu nebyl nalezen marker 'D' označující IR data.")
            return

        # Ujistíme se, že před 'D' je hlavička ST <id>
        if pos >= 3 and payload[pos-3] == 0x53 and payload[pos-2] == 0x54:
            cmd_id = payload[pos-1]
            if testing:
                print(f"[TESTING] Platná hlavička nalezena: ST, ID příkazu: {cmd_id}")
        else:
            if testing:
                print("[TESTING] Varování: Marker 'D' nalezen, ale chybí předchozí hlavička 'ST <id>'")

        # Frekvenční bajt je hned za 'D'
        freq_byte = payload[pos+1]
        if testing:
            print(f"[TESTING] Frekvenční bajt: 0x{freq_byte:02X} ({freq_byte} kHz)")

        # IR data začínají za frekvenčním bajtem
        ir_data_start = pos + 2

        # Hledáme konec "EN" (0x45, 0x4E) na konci payloadu
        try:
            end_pos = payload.index(b'EN', ir_data_start)
            ir_data = payload[ir_data_start:end_pos]
            if testing:
                print(f"[TESTING] Nalezen konec 'EN' na pozici {end_pos}. Délka IR dat: {len(ir_data)} B")
        except ValueError:
            # Pokud by náhodou EN chybělo (nemělo by), vezmeme zbytek
            ir_data = payload[ir_data_start:]
            if testing:
                print(f"[TESTING] Varování: Konec 'EN' nenalezen. Beru všechna zbývající data ({len(ir_data)} B)")

        # 1. Sloučení po sobě jdoucích bajtů se stejným stavem (ON/OFF)
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
                    print(f"  [{idx:02d}] Bajt: 0x{b:02X} -> Sloučen s předchozím (Stav: {'ON' if state_on else 'OFF'}, Celková doba: {abs(merged_pulses[-1])} us)")
            else:
                merged_pulses.append(duration if state_on else -duration)
                if testing:
                    print(f"  [{idx:02d}] Bajt: 0x{b:02X} -> Nový segment (Stav: {'ON' if state_on else 'OFF'}, Doba: {duration} us)")

        # 2. Filtrace šumu (odstranění příliš krátkých pulzů < 50us)
        pulses = []
        noise_pulses = []
        for p in merged_pulses:
            if abs(p) < 50:
                noise_pulses.append(p)
            else:
                pulses.append(p)

        if testing:
            print(f"\n[TESTING] Statistiky:")
            print(f"  Celkem surových segmentů po sloučení: {len(merged_pulses)}")
            print(f"  Počet platných pulzů (> 50 us): {len(pulses)}")
            print(f"  Odfiltrovaný šum (< 50 us): {noise_pulses}")

        if len(pulses) < 4:
            if testing:
                print("[TESTING] Příliš málo platných pulzů (< 4), ignoruji zobrazení kódu.")
            return

        print("\n" + "="*50)
        print(f" ZACHYCEN IR KÓD (Počet pulzů: {len(pulses)})")
        print("="*50)
        print("Formát pro zpětné vysílání:")
        pulse_str = ", ".join(map(str, pulses))
        print(f"[{pulse_str}]")
        print("="*50)
        
        # Přidáme zachycený kód do historie příkazů (bude dostupný přes šipku nahoru)
        try:
            readline.add_history(pulse_str)
        except Exception:
            pass

    #
    # hlavní smyčka pro učení
    #
    def learn(self, testing=False):
        self.start_receive()
        if testing:
            print("\n=== ČEKÁM NA STISK TLAČÍTKA (TESTOVACÍ REŽIM) ===")
        else:
            print("\n=== ČEKÁM NA STISK TLAČÍTKA ===")
        print("Pro návrat do hlavního menu stiskněte Ctrl+C.")

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
    # detailní výpis informací o USB zařízení (pro Testing mode)
    #
    def print_detailed_hw_info(self):
        print("\n" + "="*50)
        print(" DETAILNÍ INFORMACE O USB HARDWARU")
        print("="*50)
        
        dev = self.dev
        print(f"Výrobce:          {dev.manufacturer}")
        print(f"Produkt:          {dev.product}")
        print(f"Sériové číslo:    {dev.serial_number}")
        print(f"Vendor ID (VID):  0x{dev.idVendor:04X} ({dev.idVendor})")
        print(f"Product ID (PID): 0x{dev.idProduct:04X} ({dev.idProduct})")
        print(f"Třída zařízení:   {dev.bDeviceClass}")
        print(f"Podtřída:         {dev.bDeviceSubClass}")
        print(f"Protokol:         {dev.bDeviceProtocol}")
        print(f"Max velikost EP0: {dev.bMaxPacketSize0}")
        print(f"Počet konfigurací: {dev.bNumConfigurations}")
        
        for cfg in dev:
            print(f"\nKonfigurace {cfg.bConfigurationValue}:")
            print(f"  Popis (index): {cfg.iConfiguration}")
            print(f"  Celková délka: {cfg.wTotalLength}")
            print(f"  Počet rozhraní: {cfg.bNumInterfaces}")
            
            for intf in cfg:
                print(f"  Rozhraní {intf.bInterfaceNumber} (Alternate setting {intf.bAlternateSetting}):")
                print(f"    Třída rozhraní: {intf.bInterfaceClass}")
                print(f"    Podtřída:       {intf.bInterfaceSubClass}")
                print(f"    Protokol:       {intf.bInterfaceProtocol}")
                print(f"    Počet endpointů: {intf.bNumEndpoints}")
                
                for ep in intf:
                    print(f"    Endpoint 0x{ep.bEndpointAddress:02X}:")
                    print(f"      Směr:        {'Vstup (IN)' if usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_IN else 'Výstup (OUT)'}")
                    print(f"      Typ přenosu: {usb.util.endpoint_type(ep.bmAttributes)}")
                    print(f"      Max velikost paketu: {ep.wMaxPacketSize}")
                    print(f"      Interval dotazování: {ep.bInterval}")
        print("="*50 + "\n")

    #
    # smyčka režimu odesílání (Sending mode)
    #
    def sending_mode(self):
        print("\n=== REŽIM VYSÍLÁNÍ (Sending mode) ===")
        print("Zadejte sekvenci pulzů jako čísla oddělená čárkami nebo mezerami (např. 256, -672, 192).")
        print("Kladné hodnoty jsou pulzy (vysílač zapnut), záporné hodnoty jsou mezery (vysílač vypnut).")
        print("Pro návrat do hlavního menu stiskněte Ctrl+C, nebo zadejte 'q' / 'exit'.")
        
        default_sequence = [6944, -4576, 544, -608, 544, -1728, 544, -608, 544, -608, 544, -608, 544, -608, 544, -608, 544, -608, 544, -1728, 544, -608, 544, -1728, 592, -1680, 592, -1680, 592, -1680, 592, -560, 592, -1680, 544, -1728, 544, -608, 544, -1728, 544, -1728, 544, -608, 544, -1728, 544, -1728, 544, -608, 544, -608, 544, -1728, 544, -608, 544, -608, 544, -1728, 544, -608, 544, -608, 544, -1728, 544, -40688, 9008, -2288, 592, -304]

        while True:
            try:
                user_input = input("\nSekvence k odeslání [Enter pro výchozí]: ").strip()
                if user_input.lower() in ('q', 'exit'):
                    print("Ukončuji režim vysílání...")
                    break
                
                if not user_input:
                    pulses = default_sequence
                    print(f"Používám výchozí sekvenci ({len(pulses)} pulzů)...")
                else:
                    try:
                        pulses = self.parse_pulse_sequence(user_input)
                        if not pulses:
                            print("Sekvence je prázdná, zkuste to znovu.")
                            continue
                        # Přidáme úspěšně naparsovanou sekvenci do historie
                        readline.add_history(user_input)
                    except ValueError as ve:
                        print(f"Chyba při parsování sekvence: {ve}")
                        continue
                
                self.transmit_ir(pulses, self.default_frequency)
                
            except KeyboardInterrupt:
                # Bude zachyceno v hlavním bloku pro návrat do menu
                raise

    #
    # pomocná metoda pro naparsování sekvence pulzů z řetězce
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
                raise ValueError(f"Neplatné číslo: '{part}'")
                
        return pulses


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Ovládání Tiqiaa Tview USB IR transceiveru")
    parser.add_argument(
        "-f", "--frequency",
        type=int,
        default=255,
        help="Výchozí frekvence v kHz pro vysílání (např. 255)"
    )
    args = parser.parse_args()

    dev = None
    try:
        dev = TiqiaaTview(default_frequency=args.frequency)
        dev.init()

        while True:
            print("\n=== HLAVNÍ MENU ===")
            print("1] Scanning mode (příjem IR)")
            print("2] Sending mode (vysílání IR)")
            print("3] Testing mode (diagnostika a příjem s detaily)")
            print("Q] Konec")
            
            try:
                choice = input("Vyberte možnost (1-3, Q): ").strip().upper()
            except (KeyboardInterrupt, EOFError):
                print("\nUkončuji program...")
                break

            if choice == "1":
                try:
                    dev.learn(testing=False)
                except KeyboardInterrupt:
                    print("\nNávrat do hlavního menu...")
                    dev.set_idle()
            elif choice == "2":
                try:
                    dev.sending_mode()
                except KeyboardInterrupt:
                    print("\nNávrat do hlavního menu...")
                    dev.set_idle()
            elif choice == "3":
                try:
                    dev.print_detailed_hw_info()
                    dev.learn(testing=True)
                except KeyboardInterrupt:
                    print("\nNávrat do hlavního menu...")
                    dev.set_idle()
            elif choice in ("Q", "QUIT", "EXIT"):
                print("Ukončuji program...")
                break
            else:
                print("Neplatná volba, zkuste to znovu.")

    except RuntimeError as re:
        print("\nCHYBA:", re)
    except KeyboardInterrupt:
        print("\nKonec")
    finally:
        if dev:
            dev.close()