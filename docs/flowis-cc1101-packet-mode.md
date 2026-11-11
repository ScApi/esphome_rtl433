# Flowis+ ECO — odbiór CC1101, dekodowanie i Home Assistant

Ten dokument opisuje jedną, docelową ścieżkę odbioru nowszych modułów radiowych Flowis+ ECO:
ESP32 + CC1101, natywny sprzętowy tryb pakietowy ESPHome, dekodowanie Type-1/Type-2 i sensory
udostępniane przez API ESPHome do Home Assistant.

**Testowana konfiguracja:** ESPHome 2026.10.2, ESP32-WROOM-32 i CC1101. W dniu 2026-11-11
odbiornik uzyskał ramki Type-1 i Type-2 z poprawnym sprzętowym CRC, przy strojeniu 868.00 MHz
i RSSI około -65 dBm.

Kompletny przykład do skopiowania znajduje się w pliku
[example-flowis-cc1101-homeassistant.yaml](../example-flowis-cc1101-homeassistant.yaml).
Nie należy łączyć go z komponentem rtl433 na tym samym module CC1101.

## 1. Kompatybilność — generacje modułów i oprogramowanie

Wątek [merbanan/rtl_433 #2317](https://github.com/merbanan/rtl_433/issues/2317) zawiera obserwacje
kilku generacji. Nie jest to oficjalna tabela kompatybilności producenta — poniższe informacje
mają status opisanych testów społeczności.

| Oznaczenie modułu / data na etykiecie | Seria numerów podana w wątku | Stan rozpoznania | Zalecana ścieżka |
|---|---|---|---|
| v24.8.00, 03/2022 | 2404… | Użytkownik zgłosił poprawne dekodowanie przez starszy wbudowany dekoder Flowis w rtl_433. | Klasyczny dekoder rtl_433 / rtl_433_ESP dla zgodnych ramek. |
| v24.C.06, 02/2023 | 2405… | Zgłoszono brak dekodowania przez ówczesny dekoder rtl_433. Przyczyna nie została rozstrzygnięta. | Brak potwierdzonej zgodności w tej konfiguracji. |
| v24.9.00, 06/2023 | 2406… | Zgłoszono brak dekodowania przez ówczesny dekoder rtl_433. Nie dowodzi to szyfrowania. | Brak potwierdzonej zgodności w tej konfiguracji. |
| **v24.C.11, 1/2026** | **2409xxxxx** | Autor wątku potwierdził odbiór niezaszyfrowanych ramek. Ten sam PHY, sync word i CRC; Type-1 ma 25 bajtów, Type-2 19 bajtów. |
| **v24.C.11, 6/2026** | **2410xxxxx** | Ten sam PHY, sync word i CRC; Type-1 ma 25 bajtów, Type-2 19 bajtów. |

Wpis 1/2026 oznacza datę widoczną na etykiecie modułu radiowego; v24.C.11 to jego oznaczenie wersji.
Nie oznacza to, że każdy licznik wyprodukowany od stycznia 2026 r. został przetestowany.
Potwierdzenie dotyczy konkretnego modułu v24.C.11 z serii 2409xxxxx i ramek pokazanych w wątku.

### Jakie oprogramowanie dekoduje poszczególne typy?

- **Wbudowany dekoder Flowis w rtl_433:** obsługa Type-1 została dodana w 2023 r. (PR
  [#2357](https://github.com/merbanan/rtl_433/pull/2357)). Historyczny dekoder rozpoznaje starszy
  układ Type-1; Type-2 nie był przez niego dekodowany.
- **Dekodowanie Type-2 w rtl_433:** PR
  [#3785](https://github.com/merbanan/rtl_433/pull/3785) dodaje siedem miesięcznych wartości Type-2.
  Stan PR sprawdzony 2026-11-11: **otwarty**, więc nie należy zakładać, że poprawka znajduje się
  w standardowym wydaniu rtl_433.
- **Ten przykład ESPHome:** nie korzysta z dekodera rtl_433. Odbiera ramki przez sprzętowy packet
  mode CC1101, a następnie dekoduje Type-1 i Type-2 w lambdzie ESPHome. Test wykonano na ESPHome
  2026.10.2. Komponent CC1101 jest override'em wersji wbudowanej w tę wersję ESPHome.

Nie należy utożsamiać braku dekodowania przez starszy SDR/rtl_433 z szyfrowaniem. Raport dla
v24.C.11 wyraźnie potwierdza brak szyfrowania i poprawny sprzętowy CRC.

## 2. Ustawienia radia

Dla opisanej generacji protokół radiowy ma następujące parametry:

| Parametr | Wartość |
|---|---|
| Modulacja | 2-FSK |
| Szybkość symbolowa | 100 kbaud |
| Odchylenie FSK | 50 kHz |
| Sync word | D391D391 |
| Tryb synchronizacji | 30/32 |
| Długość pakietu | zmienna |
| CRC | CRC-16, wielomian 0x8005, init 0xFFFF |
| Pasmo odbiornika | 325 kHz w sprawdzonym profilu |
| IF | 203 kHz w sprawdzonym profilu |

Ważne: wątek opisuje nośną około 868.044 MHz, ale rzeczywiste strojenie zależy od egzemplarza
modułu i lokalnego odbiornika. W testowanej instalacji użytkownika poprawne ramki CRC uzyskano
przy 868.00 MHz. **Nie podmieniaj automatycznie częstotliwości działającej instalacji na 868.04 MHz.**
Najpierw potwierdź odbiór poprawnego CRC i stabilny RSSI po każdej zmianie częstotliwości.

### Konfiguracja ESPHome

W pliku urządzenia użyj źródła komponentu z tej gałęzi:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/ScApi/esphome_rtl433
      ref: experiment/flowis-cc1101-packet-diagnostics
      path: ha-external-components
    components: [cc1101]
    refresh: 0s
```

Następnie konfiguracja radia:

```yaml
spi:
  clk_pin: GPIO18
  miso_pin: GPIO19
  mosi_pin: GPIO23

cc1101:
  cs_pin: GPIO5
  gdo0_pin: GPIO4
  frequency: 868.00MHz
  modulation_type: 2-FSK
  symbol_rate: 100000
  fsk_deviation: 50kHz
  filter_bandwidth: 325kHz
  if_frequency: 203kHz
  sync_mode: 30/32
  sync1: 0xD3
  sync0: 0x91
  packet_mode: true
  packet_length: 0
  crc_enable: true
```

Wartość 868.00 MHz jest wartością zweryfikowaną w tej konkretnej instalacji, a nie uniwersalną
częstotliwością gwarantowaną dla wszystkich modułów.

Podłączenie użyte w przykładzie ESP32-WROOM-32:

| CC1101 | ESP32 |
|---|---:|
| CSN | GPIO5 |
| GDO0 | GPIO4 |
| SCK | GPIO18 |
| MISO | GPIO19 |
| MOSI | GPIO23 |
| VCC | 3.3 V |
| GND | GND |
| GDO2 | nieużywany |

**Nie dodawaj** równolegle komponentu rtl433 lub rtl_433_ESP. Natywny CC1101 packet mode i
sterownik rtl433 nie mogą jednocześnie obsługiwać tego samego modułu po SPI.

## 3. Co zmienić, aby dekodować stan licznika

W pliku `example-flowis-cc1101-homeassistant.yaml` filtr identyfikatora znajduje się wewnątrz
`cc1101:` → `on_packet:` → `lambda:`. Wyszukaj stałą:

```cpp
constexpr uint32_t EXPECTED_METER_ID = 241047300UL;
```

Zmień wyłącznie liczbę `241047300` na identyfikator swojego licznika (zachowaj końcówkę `UL`).
Nie zmieniaj kolejności bajtów w dekoderze — konwersja ID z ramki jest już little-endian.

W aktualnie przechwyconych ramkach identyfikator jest zapisany jako bajty
`04 17 5E 0E` w kolejności little-endian, co daje **241047300**.
Wcześniejsze ID 241047064 pochodziło z błędnej konfiguracji Home Assistant i nie jest używane
przez nowy dekoder.

Jeżeli kiedyś trzeba będzie uruchomić odbiór diagnostyczny bez filtrowania identyfikatora,
można ustawić EXPECTED_METER_ID na 0. W normalnej pracy lepiej pozostawić filtr włączony,
aby nie łączyć danych z kilku liczników znajdujących się w zasięgu.

### Type-1 — stan całkowity i status

Callback ESPHome otrzymuje dane bez bajtu długości oraz bez dwóch bajtów CRC.
Dla aktualnej ramki Type-1 payload ma 25 bajtów, pierwszy bajt to 0x01.

| Indeks w x | Znaczenie | Sposób przeliczenia |
|---|---|---|
| 0 | Typ wiadomości 0x01 | identyfikacja Type-1 |
| 1–4 | ID licznika | uint32 little-endian |
| 5 | bajt stały / nieznany w tym układzie | zachować, nie interpretować |
| 6–9 | czas urządzenia | bit-packed; dekoder używa układu upstream Flowis |
| 10–12 | stan całkowity w litrach | uint24 little-endian / 1000 = m³ |
| 13 | surowy status backflow | znaczenie surowych bitów pozostaje do potwierdzenia |
| 14 | surowy bajt alarmu | znaczenie surowych bitów pozostaje do potwierdzenia |
| 15–20 | pola dodatkowe | nieinterpretowane |
| 21–24 | stan na koniec poprzedniego miesiąca w litrach | uint32 little-endian / 1000 = m³ |

Zużycie bieżącego miesiąca można obliczyć z Type-1:
stan całkowity minus stan skumulowany na koniec poprzedniego miesiąca.

### Type-2 — zużycie miesięczne

Type-2 ma 19 bajtów payloadu i zaczyna się od 0x02.

| Indeksy w x | Znaczenie |
|---|---|
| 0 | Typ wiadomości 0x02 |
| 1–4 | ID licznika, uint32 little-endian |
| 5–18 | siedem wartości uint16 little-endian w litrach |

Kolejność siedmiu wartości: bieżący miesiąc, poprzedni miesiąc i kolejnych pięć wcześniejszych
miesięcy — razem siedem miesięcy. Każdą prawidłową wartość należy podzielić przez 1000, aby
otrzymać m³. Wartość 0xFFFF jest traktowana jako wartość specjalna/nieprawidłowa i nie jest
publikowana jako 65.535 m³. Zwykłe zero może być prawidłową wartością, np. na początku nowego
miesiąca.

## 4. Przykładowe wartości z odbioru użytkownika

Ramka Type-1 z 2026-11-11:

```
01 04 17 5E 0E C1 98 93 D6 6A 7B 9C 00 00 01 00 00 A5 0B 00 80 E9 8F 00 00
```

Zdekodowane wartości:

- ID: 241047300
- czas urządzenia: 2026-11-11 09:14:24
- stan całkowity: 40.059 m³
- stan skumulowany na koniec poprzedniego miesiąca: 36.841 m³
- różnica bieżącego miesiąca: 3.218 m³

Ramka Type-2:

```
02 04 17 5E 0E 92 0C DE 2B 93 27 79 29 04 12 01 00 FF FF
```

Zużycie miesięczne w kolejności od bieżącego miesiąca:

**3.218; 11.230; 10.131; 10.617; 4.612; 0.001 m³**, a ostatnia pozycja jest oznaczona 0xFFFF
i nie jest publikowana.

Wartości bieżącego miesiąca z obu ramek są zgodne:
40.059 m³ - 36.841 m³ = 3.218 m³. W ESPHome są publikowane osobno:
sensor Flowis zużycie bieżący miesiąc (Type-2) oraz sensor Flowis bieżący miesiąc kontrola
(obliczony z Type-1). Pozostawienie dwóch encji ułatwia wykrycie rozbieżności w przyszłych odczytach.

36.841 m³ z Type-1 to stan skumulowany licznika
na koniec poprzedniego miesiąca, a nie zużycie w tym miesiącu. Type-2 podaje zużycie za
poprzedni miesiąc jako osobną wartość — w pokazanej ramce jest to 11.230 m³. Jeśli rachunek
za miesiąc wskazuje 36 m³ zużycia, a nie stan licznika, tych wartości nie należy uznawać
za zgodne. Trzeba porównać okres rozliczeniowy, numer licznika i wskazania z rachunku.

## 5. Encje ESPHome i Home Assistant

Pełny YAML tworzy sensory:

- stan całkowity w m³ z klasą total_increasing;
- skumulowany stan na koniec poprzedniego miesiąca;
- bieżący miesiąc z Type-2 oraz osobny sensor kontrolny wyliczony z Type-1;
- sześć wcześniejszych wartości miesięcznych z Type-2;
- RSSI, LQI i przesunięcie częstotliwości;

Klasy `total_increasing` i `device_class: water` są przypisane do skumulowanego stanu licznika
oraz skumulowanego wskazania na koniec poprzedniego miesiąca. Sensory zużycia miesięcznego i
sensor kontrolny nie mają klasy licznika narastającego ani klasy urządzenia water, ponieważ
ich wartości dotyczą okresu miesięcznego i mogą zmienić się/wyzerować przy granicy miesiąca.
- surowe bajty alarmu i backflow;
- numer i czas zapisany w liczniku.

Po dodaniu urządzenia sprawdź rzeczywiste identyfikatory encji w Home Assistant:
**Narzędzia deweloperskie → Stany**.

### Zachowanie ostatniej poprawnej wartości w Home Assistant

Ponieważ poprzedni sensor był oparty na błędnym ID i pokazywał około 6 m³, nie należy podmieniać
jego źródła i zachowywać tego samego unique_id licznika total_increasing bez kontroli historii.
Home Assistant mógłby uznać skok z około 6 do 40.059 m³ za rzeczywiste zużycie wody i włączyć
go do statystyk długoterminowych.

Na czas weryfikacji utwórz osobny sensor z nowym unique_id:

```yaml
template:
  - sensor:
      - name: "Licznik wody (ostatni odczyt Flowis)"
        unique_id: licznik_wody_ostatni_odczyt_flowis
        unit_of_measurement: "m³"
        device_class: water
        state_class: total_increasing
        state: >
          {% set current = states('sensor.flowis_stan_calkowity') %}
          {% if is_number(current) and current | float > 0 %}
            {{ current | float }}
          {% elif is_number(this.state) and this.state | float > 0 %}
            {{ this.state | float }}
          {% else %}
            {{ none }}
          {% endif %}
```

Jeśli w configuration.yaml istnieje już sekcja template:, dodaj sensor pod tą sekcją zamiast
tworzyć drugą sekcję o tej samej nazwie. Ten szablon przyjmuje tylko dodatnią wartość liczbową,
a w przypadku unknown, unavailable, pustego stanu lub zera zachowuje poprzedni dodatni odczyt.
Przed pierwszym poprawnym odczytem pozostaje unknown, nie tworzy sztucznego zera.

Po potwierdzeniu zgodności odczytu z fizycznym licznikiem można przełączyć dashboardy i
automatyzacje na nowy sensor. Starej historii statystyk nie należy łączyć automatycznie
z nowym licznikiem bez przemyślanego resetu/migracji.

## 6. Instalacja i weryfikacja

1. Użyj ESPHome 2026.10.2.
2. W istniejącym pliku flowis.yaml zachowaj swoje dane Wi-Fi, API, OTA i konfigurację ESP32.
3. Dodaj override external_components i konfigurację cc1101 z rozdziału 2.
4. Skopiuj z kompletnego przykładu blok on_packet oraz sekcje sensor i text_sensor.
5. Upewnij się, że masz tylko jeden wpis sensor: i jeden text_sensor: na głównym poziomie YAML.
6. Uruchom Validate przed kompilacją, następnie zainstaluj firmware i sprawdź logi.
7. Oczekuj FLOWIS_DIAG PACKET_ACCEPT z crc_en=1, crc_ok=1, Type-1 len=25 i Type-2 len=19 oraz logów flowis_decode.
8. Porównaj stan całkowity, numer ID i okresy miesięczne z fizycznym licznikiem oraz dokumentem rozliczeniowym.

Nie interpretuj surowych alarm/backflow ani specjalnego 0xFFFF jako rzeczywistych odczytów.
Dekoder nie publikuje niepotwierdzonych typów ramek.

## Źródła i status implementacji

- [rtl_433 issue #2317 — obserwacje modułu v24.C.11 z etykietą 1/2026](https://github.com/merbanan/rtl_433/issues/2317)
- [rtl_433 PR #2357 — oryginalny dekoder Flowis Type-1](https://github.com/merbanan/rtl_433/pull/2357)
- [rtl_433 PR #3785 — dekodowanie siedmiu miesięcznych wartości Type-2](https://github.com/merbanan/rtl_433/pull/3785); sprawdzony 2026-11-11 jako otwarty
- [Kompletny przykład ESPHome z dekoderem i sensorami](../example-flowis-cc1101-homeassistant.yaml)

Ten przykład jest implementacją społecznościową opartą na przechwyconych ramkach i nie stanowi
certyfikowanego dekodera producenta licznika.
