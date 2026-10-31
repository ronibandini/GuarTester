# 🎲 GUAR Tester

### True Random Number Generator Experiment 

GUAR Tester (Generador Uniforme de Anomalías Random) is an experimental device designed to measure statistical deviations in random binary sequences under different intention conditions.

Built around the Arduino UNO R4 Minima, it uses the Renesas RA4M1 microcontroller's hardware true random number generation capabilities to produce random bits, count zeros and ones, and analyze the results using statistical measurements.

## ✨ Features

- 🎲 Hardware True Random Number Generation (TRNG)
- 📊 Zero and one counting per trial
- 📈 Cumulative statistical analysis
- 🧮 Z-score and p-value calculations
- 🖥️ Serial Monitor output with CSV records

## 🧰 Hardware

- Arduino UNO R4 Minima
- TM1637 four-digit seven-segment display
- Potentiometer
- Momentary push button
- Connecting wires
- 9 volts battery

### Main components

The Arduino UNO R4 Minima uses the Renesas RA4M1 microcontroller, which integrates the SCE5 security engine and hardware-based true random number generation functionality.

## 🔌 Connections

| Component | Arduino pin |
|---|---|
| TM1637 CLK | D2 |
| TM1637 DIO | D3 |
| Push button | D4 |
| Potentiometer | A0 |
| Button ground | GND |

The push button uses the internal `INPUT_PULLUP` configuration. The potentiometer selects the experimental condition.

## 🧪 Experimental conditions

The potentiometer selects one of three modes:

| Condition | Intention |
|---|---|
| LOW | Intend to obtain more zeros |
| BASELINE | No specific intention |
| HIGH | Intend to obtain more ones |

Each session consists of **60 trials**, with **200 random bits per trial**.

## ⚙️ How it works

1. The hardware TRNG generates random bits.
2. The firmware counts zeros and ones.
3. Each trial is compared against the expected distribution.
4. Cumulative statistics are maintained for the selected condition.
5. The firmware calculates statistical deviations and classifies the results.
6. Results are displayed on the seven-segment display and sent to the Serial Monitor.

## 📊 Statistical analysis

For a binary sequence with equally probable zeros and ones, the expected distribution is 50% zeros and 50% ones.

The firmware evaluates deviations using a Z-score, comparing the observed count against the expected count under a binomial model.

The results include:

- Number of zeros and ones in the trial
- Cumulative zero and one counts
- Expected count
- Statistical deviation
- Z-score
- p-value
- Condition-specific interpretation

The classification threshold is configurable in the firmware.

These measurements describe statistical deviations in the generated sequence. They do not, by themselves, establish that intention influences random number generation.

## 🖥️ Serial Monitor

The Serial Monitor provides structured records for trial results, session information, condition changes, and cumulative statistics.

CSV output can be saved for subsequent analysis using Python, spreadsheets, or statistical software.

## 🔧 Configuration

The result-reading pause can be adjusted in the firmware:

```cpp
const uint16_t resultReadPauseMs = 3000;
```

The default value is 3000 milliseconds (3 seconds).

Other experimental parameters, including trial size, session length, and statistical thresholds, can be adjusted in the corresponding firmware settings.

## 🚀 Upload

1. Install the Arduino IDE.
2. Select **Arduino UNO R4 Minima** as the target board.
3. Install the required libraries:
   - [TM1637Display](https://github.com/avishorp/TM1637)
   - [TRNG library](https://github.com/embarquech/trng)
4. Open `guarX.ino`.
5. Connect the board over USB.
6. Compile and upload the firmware.
7. Open the Serial Monitor to inspect the experimental output.

## 🧪 Project status

GUAR Tester is an experimental maker project exploring hardware randomness, statistical deviations, and intention-based testing.

The device provides a reproducible procedure for collecting and analyzing binary sequences under different experimental conditions.

## ♻️ Repurposed Enclosure

GUAR Tester reuses the enclosure of an Italian device originally designed to measure soil parameters. Its original electronics were replaced with an Arduino UNO R4 Minima and a hardware true random number generator. The name was also re-signified: **GUAR** became the Spanish acronym for *Generador Uniforme de Anomalías Random*, transforming an agricultural measuring instrument into an experimental device for studying randomness and statistical deviations.

## 👤 Author


**Roni Bandini** — Buenos Aires, Argentina

- GitHub: [@ronibandini](https://github.com/ronibandini)
- Medium: [bandini.medium.com](https://bandini.medium.com/)
- X / Twitter: [@RoniBandini](https://x.com/RoniBandini)

Part of the Contracultura Maker universe.

