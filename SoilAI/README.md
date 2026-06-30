# SoilAI — AI-Driven Prediction of Soil Nutrient Availability

Step 1 of the build: project skeleton + SQLite-backed Login/Register.

## Folder structure

```
SoilAI/
├── CMakeLists.txt
├── include/
│   ├── core/        -> User.h
│   ├── db/           -> DatabaseManager.h
│   ├── ui/            -> LoginWindow.h
│   ├── sensors/       (next module)
│   ├── ai/            (next module)
│   └── reports/       (next module)
├── src/
│   ├── main.cpp
│   ├── db/DatabaseManager.cpp
│   └── ui/LoginWindow.cpp
├── resources/        (icons, qss themes — populated later)
└── assets/           (logo, fonts — populated later)
```

## What's implemented

1. **CMakeLists.txt** — links Qt6 Core/Gui/Widgets/Charts/Sql/SerialPort/PrintSupport
   and SQLite3, globs all sources, copies `resources/` and `assets/` next to the binary.
2. **DatabaseManager** (singleton) — opens `soilai.db`, creates the full schema
   (Users, SensorReadings, Predictions, Recommendations, Reports) with foreign keys,
   and exposes `registerUser`, `authenticate` (SHA-256 + per-user salt),
   `insertSensorReading`, `fetchReadings`, `deleteReading`, `insertPrediction`,
   `insertRecommendation`, `insertReportRecord`. Throws `DatabaseException` on failure.
3. **User** — small encapsulated data model returned by `authenticate()`.
4. **LoginWindow** (QDialog) — dark-themed card UI with Username, Password
   (show/hide toggle), Remember-me (via QSettings), Login, Register. Talks only to
   `DatabaseManager`; no SQL in the UI layer (clean MVC separation).
5. **main.cpp** — initializes the DB, shows LoginWindow, and on success is ready to
   hand off to `MainWindow` (Dashboard), which is the next module.

## How it works

- On launch, `DatabaseManager::initialize()` opens/creates `soilai.db` and runs
  `CREATE TABLE IF NOT EXISTS` for every table, so the first run self-provisions.
- `LoginWindow::onRegisterClicked()` generates a random 16-char salt, hashes
  `password + salt` with SHA-256, and stores both salt and hash — never the
  plaintext password.
- `LoginWindow::onLoginClicked()` re-hashes the entered password with the stored
  salt and compares hashes; on match it calls `accept()`, and `main.cpp` reads
  `login.authenticatedUser()`.
- "Remember me" only persists the **username** (via `QSettings`) — never the
  password — and pre-fills the username field on next launch.

## Build instructions

Requires Qt 6 (Core, Widgets, Charts, Sql, SerialPort, PrintSupport modules) and SQLite3 dev headers.

```bash
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64
cmake --build .
./SoilAI
```

## Next modules (in order)

1. ~~MainWindow + Dashboard~~ ✅ done
2. ~~Sensor + SensorSimulator~~ ✅ done
3. ~~SerialPortManager~~ ✅ done
4. ~~AIEngine~~ ✅ done (this step)
5. **RecommendationEngine** — fertilizer rules engine, consumes AIEngine's last prediction.
6. **WeatherManager** — simulated weather, OpenWeather-ready interface.
7. **HistoryManager + ChartManager** — Qt Charts trends, filters, CSV export.
8. **ReportGenerator** — QPrinter/QPdfWriter PDF report with charts and logo.
9. **SettingsManager** — theme, COM port, baud rate, language, persisted via QSettings.

## Step 4 additions (this update)

- **AIEngine** (`include/ai`, pure C++ — no Qt Widgets dependency, so it's trivially unit-testable):
  - `PredictionInput` carries every contextual field from the brief: pH, N, P, K, Temperature,
    Humidity, Rainfall, Soil Type, Crop Type.
  - `predict()` simulates an XGBoost-style regressor with weighted, domain-informed equations:
    - **pH penalty**: nutrient availability falls off the further pH is from ~6.5 (real agronomy
      rule — phosphorus especially locks up outside pH 6–7.5).
    - **Leaching loss**: rainfall + humidity reduce available N and K (both are mobile in soil
      water), applied as a multiplicative loss factor.
    - **Soil type factor**: Sandy/Loamy/Clayey/Black/Red soils get different retention multipliers.
    - **Soil Health Score (0-100)**: weighted composite of pH closeness-to-optimum, N/P/K
      sufficiency, and moisture balance — mapped to a label (Excellent/Good/Moderate/Poor/Critical).
    - **Confidence (0-1)**: degrades for extreme pH, very high rainfall, or extreme temperature —
      mimicking how a real model's prediction interval widens outside its training distribution.
  - `loadCoefficients(path)` reads a simple `key=value` text file to override any default weight —
    this is the seam where a real trained XGBoost model's exported parameters would be plugged in
    later without changing any calling code.
  - `PredictionResult::toJson()` serializes the result for storage in
    `Predictions.predicted_nutrients_json`.
- **MainWindow::onPredictNutrients()** is now fully wired:
  1. Reads the latest live `SensorSnapshot` from `Dashboard`.
  2. Prompts for the 3 contextual inputs the engine needs but sensors can't provide
     (rainfall, soil type, crop type) via lightweight `QInputDialog`s.
  3. Runs `AIEngine::predict()`.
  4. Persists both the `SensorReadings` row and the linked `Predictions` row via
     `DatabaseManager::insertSensorReading` / `insertPrediction`.
  5. Shows predicted N/P/K, Soil Health Score + label, and confidence in a result dialog.
  6. Caches the result (`m_lastPrediction`) so **Recommend Fertilizer** (next module) can consume
     it directly instead of recomputing.

Say "next module" / "RecommendationEngine" and I'll build the fertilizer rules engine next —
it'll read `m_lastPrediction` plus crop/soil context and populate the `Recommendations` table
(fertilizer, reason, quantity, application time, priority).


- **SerialPortManager** — wraps `QSerialPort`. Connects to a named COM/tty port at a given baud
  rate, buffers partial reads until a `\n`-terminated line arrives, and parses
  `PH:6.8,N:120,P:40,K:160,T:31,H:65` into a `SensorSnapshot` — the exact same struct
  `SensorSimulator` emits.
  - **Auto-reconnect**: on any `QSerialPort` error (cable unplugged, device busy, etc.) or if a
    data watchdog timer (5s) sees no valid line while "connected", it closes the port, flips
    `connectionStateChanged(false)`, and retries on a 3s timer until the port reopens.
  - **Format tolerance**: unknown keys are ignored, malformed numbers are skipped per-field rather
    than discarding the whole line, and `parseError(line, reason)` is emitted for lines with no
    recognizable fields at all (useful for a future serial-monitor debug panel).
  - `SerialPortManager::availablePorts()` is a static helper (`QSerialPortInfo`) used to populate
    the Dashboard's port dropdown and will later feed the Settings dialog's COM Selection field.
- **Dashboard** now has a **data source selector** in the header: "Simulated" vs
  "Live Arduino (Serial)", plus a port dropdown that appears only in serial mode.
  - Start/Stop now route to whichever source is active — `SensorThreadController` for Simulated,
    `SerialPortManager::connectToPort/disconnectPort` for Serial.
  - Both sources feed the identical `onSnapshot(SensorSnapshot)` slot, so **zero gauge code
    changed** — this is the substitutability the architecture was designed for in Step 2.
  - The status pill now also reflects serial states: `ARDUINO CONNECTED`,
    `ARDUINO DISCONNECTED — reconnecting…`, `SERIAL ERROR — retrying…`, `NO COM PORT SELECTED`.

## Wiring your real Arduino

Upload a sketch to your Arduino that prints one line per second to `Serial`, e.g.:

```cpp
void loop() {
  Serial.print("PH:"); Serial.print(phValue, 1);
  Serial.print(",N:"); Serial.print(nValue, 0);
  Serial.print(",P:"); Serial.print(pValue, 0);
  Serial.print(",K:"); Serial.print(kValue, 0);
  Serial.print(",T:"); Serial.print(tempValue, 1);
  Serial.print(",H:"); Serial.println(humValue, 1);
  delay(1000);
}
```

Then in the app: switch the header dropdown to **Live Arduino (Serial)**, pick the COM port, and
click **Start Reading**. No other code changes are needed — `SerialPortManager` and
`SensorSimulator` are interchangeable from Dashboard's point of view.

Say "next module" / "AIEngine" and I'll build the XGBoost-style prediction engine next, wired to
the **Predict Nutrients** button and to the `Predictions` table already in the schema.


- **Sensor.h** — abstract `Sensor` base class + 6 concrete subclasses (PH/N/P/K/Temperature/Humidity),
  each just declaring its valid range; `readValue()` does a realistic random-walk simulation shared
  by all of them (inheritance + polymorphism).
- **SensorSimulator** (QObject) — owns one of each sensor, ticks every 1s via QTimer, emits
  `snapshotReady(SensorSnapshot)`.
- **SensorThreadController** — RAII wrapper that moves `SensorSimulator` onto a dedicated `QThread`
  so sensor generation never blocks the UI thread; cleanly quits/waits the thread in its destructor.
- **GaugeWidget** — custom `QPainter`-based circular gauge (arc fill + center value + label), used
  for all 6 live sensor cards — gives the "industrial SCADA" look without extra dependencies.
- **Dashboard** — central widget with header bar (user, date/time, location, weather, device status),
  the 6-gauge sensor grid, and the action bar (Start/Stop/Save/Predict/Recommend/Report/Export/Settings).
  It only depends on `SensorSnapshot`, so swapping in `SerialPortManager` later requires zero Dashboard changes.
- **MainWindow** — hosts `Dashboard` as central widget, wires `requestSaveData` to
  `DatabaseManager::insertSensorReading`, and stubs out Predict/Recommend/Report/Export/Settings
  with placeholder dialogs until those modules are generated.
- **main.cpp** — now launches `MainWindow` directly after a successful login.

## How the live data flow works

1. `MainWindow` constructs `Dashboard(currentUser)`.
2. `Dashboard`'s constructor creates a `SensorThreadController`, which spins up a background
   `QThread` running `SensorSimulator`.
3. Clicking **Start Reading** calls `SensorThreadController::startReading()`, which queues a call
   onto the simulator's thread to start its `QTimer`.
4. Every second, `SensorSimulator::tick()` reads all 6 sensors and emits `snapshotReady(SensorSnapshot)`
   — Qt automatically marshals this cross-thread signal onto the GUI thread (queued connection).
5. `Dashboard::onSnapshot()` updates the 6 `GaugeWidget`s.
6. Clicking **Save Data** emits `Dashboard::requestSaveData(snapshot)`, which `MainWindow::onSaveData()`
   persists via `DatabaseManager::insertSensorReading()`.

Say "next module" / "SerialPortManager" and I'll wire in real Arduino serial communication next,
with auto-reconnect and the same `SensorSnapshot` signal Dashboard already consumes.

