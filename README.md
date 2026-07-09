# Tiqiaa Tview IR Transceiver (Python Control)

Tento projekt obsahuje Python skript pro ovládání infračerveného (IR) transceiveru (přijímače a vysílače) **Tiqiaa Tview** přes USB rozhraní pomocí knihovny `PyUSB`.

---

## 🛠️ Hardwarové označení zařízení (HW info)

Při připojení k počítači se zařízení hlásí s následujícími identifikátory:

| Parametr | Hodnota | Poznámka |
| :--- | :--- | :--- |
| **Název zařízení** | Tiqiaa Tview USB IR Transceiver | Často označováno také jako *ZaZaRemote USB OTG dongle* nebo *Tiqiaa USB IR Blaster* |
| **Vendor ID (VID)** | `0x10C4` | Silicon Laboratories, Inc. (Cygnal Integrated Products) |
| **Product ID (PID)** | `0x8468` | Vlastní ID používané pro toto IR zařízení |
| **Integrovaný čip** | Silicon Labs custom USB HID | Upravená verze CP210x rodiny, často se hlásí jako *SLAB HIDtoSPI Slave* nebo *HIDtoUART* |
| **USB Interface** | `0` | Hlavní komunikační rozhraní |
| **USB Endpoints** | `EP_OUT = 0x01`<br>`EP_IN = 0x81` | Směr zápisu (vysílání) a čtení (příjem) dat |

> [!IMPORTANT]
> Jelikož se zařízení chová jako proprietární USB HID (Human Interface Device) a nikoliv jako standardní virtuální sériový port (COM/ttyUSB), standardní ovladače pro CP210x pro něj nebudou fungovat. Komunikace se skriptem probíhá napřímo přes nízkoúrovňové USB rozhraní.

---

## 📋 Popis projektu

Projekt se skládá ze dvou hlavních souborů:
1. [tiqiaa_IR.py](file:///home/hikaru/Source/IR/tiqiaa_IR.py) – Hlavní Python třída a obslužný kód, který implementuje komunikační protokol Tiqiaa (příkazy zabalené do `ST <id> <cmd> EN` s fragmentací po 56 bajtech).
2. [run.sh](file:///home/hikaru/Source/IR/run.sh) – Jednoduchý spouštěcí bash skript.

### Hlavní funkce:
* **Inicializace a verifikace:** Navázání spojení se zařízením, odpojení případného kernel driveru a vyžádání verze firmware (`V`).
* **Režim učení (Příjem IR):** Přepnutí transceiveru do režimu příjmu (`R`), cyklické dotazování na nová IR data (`O`), dekódování zachycených pulzů (převod surových bajtů na mikrosekundy) a filtrace šumu. Skript následně vypíše pole pulzů připravené pro zkopírování do vysílače.
* **Režim vysílání (Vysílání IR):** Přepnutí transceiveru do režimu vysílání (`S`), převod zadaných délek pulzů (v $\mu s$) na nativní jednotky zařízení (16 $\mu s$ bloky) a odeslání signálu (`D`) s nosnou frekvencí (např. 38 kHz).

---

## 🚀 Jak to zprovoznit

### 1. Požadavky
* **Python 3**
* **PyUSB** knihovna. Nainstalujte ji pomocí:
  ```bash
  pip install pyusb
  ```
* **libusb** (většinou je již součástí Linuxových distribucí, případně nainstalujte přes balíčkovací systém, např. `sudo apt install libusb-1.0-0`).

### 2. Oprávnění v Linuxu (udev pravidlo)
Aby mohl skript přistupovat k USB zařízení bez nutnosti spouštění pod `sudo`, vytvořte udev pravidlo:

1. Vytvořte soubor `/etc/udev/rules.d/99-tiqiaa.rules` s tímto obsahem:
   ```udev
   SUBSYSTEM=="usb", ATTR{idVendor}="10c4", ATTR{idProduct}="8468", MODE="0666", GROUP="plugdev"
   ```
2. Znovu načtěte pravidla:
   ```bash
   sudo udevadm control --reload-rules && sudo udevadm trigger
   ```
3. Ujistěte se, že váš uživatel je ve skupině `plugdev`.

### 3. Spuštění
Spusťte skript pomocí přiloženého shell skriptu:
```bash
./run.sh
```


## 🔗 Užitečné zdroje

* [Reverzní inženýrství protokolu Tiqiaa Tview (Habr)](https://habr.com/ru/articles/494800/) – Detailní analýza a rozbor USB komunikace a protokolu tohoto IR transceiveru.

