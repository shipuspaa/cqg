# CQG

Small service that connects to Binance WebSocket trade streams, aggregates per-symbol statistics in time windows, and writes them to a file. It uses a producer/consumer queue: network thread pushes trades, reader thread aggregates them.

## Features
- WebSocket TLS client (Boost.Asio/Beast)
- JSON parsing (nlohmann/json)
- Per-symbol aggregation in time windows (by trade timestamp)
- File output in required format
- Graceful shutdown on SIGINT/SIGTERM, reload on SIGHUP
- Unit tests (GTest)

## Requirements
- CMake 3.15+
- C++17 compiler
- Conan 2.x
- OpenSSL, Boost, nlohmann_json, GTest (via Conan)

## Build (local)
```bash
conan profile detect --force
conan install . --build=missing
cmake -S . -B build
cmake --build build -j
```

## Run
```bash
./build/cqg
```
The app reconnects automatically if the stream drops. Output is written to aggregates.log (or the configured filename).
## Docker
Build the image:
```bash
docker build -t cqg .
```

### Option 1: Run with custom config and persistent output
Mounts your local `config.json` and `output` directory for customization and data persistence:
```bash
docker run -v $(pwd)/config.json:/app/config.json -v $(pwd)/output:/app/output cqg
```
- Use this for development/testing with your own config and to save logs/files on host.

### Option 2: Run with default config (ephemeral)
Uses built-in config (`console_report=true` for console logs), output is lost after container stops:
```bash
docker run --rm cqg
```

### Docker log rotation
```bash
docker run -d --log-opt max-size=10m --log-opt max-file=10 cqg
```
### Config
Default config file: config.json

```json
{
  "trade_pairs": ["btcusdt", "ethusdt"],
  "ws": {
    "host": "stream.binance.com",
    "port": "9443",
    "handshake_timeout_sec": 10,
    "idle_timeout_sec": 10
  },
  "retry": {
    "base_retry_sec": 1,
    "max_retry_sec": 30,
    "max_retry_attempts": 32
  },
  "agg": {
    "period_ms": 1000,

  },
  "output": {
    "write_period_ms": 5000,
    "write_delay_ms": 0,
    "filename": "aggregates.log",
    "max_file_mb": 10,
    "max_files": 10,
    "console_report": false
  }
}
```

### CLI overrides
- --config=/path/to/config.json
- --trade-pairs=btcusdt,ethusdt
- --agregate-period-ms=1000
- --write-period-ms=5000
- --agregate-using-timestamp=0/1
- --write-delay-ms=0
- --output-filename=/path/to/aggregates.log
- --max-file-mb=10
- --max-files=10
- --console-report=0/1

## Tests
```bash
./build/unit_tests
```

## systemd example
Create a unit file, for example /etc/systemd/system/cqg.service:

```ini
[Unit]
Description=CQG Binance Aggregation Service
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=/opt/cqg
ExecStart=/opt/cqg/cqg --config=/opt/cqg/config.json
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now cqg
journalctl -u cqg -f
```

### Journald log rotation
systemd writes stdout/stderr to journald. Configure rotation in /etc/systemd/journald.conf, for example:

```ini
SystemMaxUse=200M
SystemMaxFileSize=10M
```

Apply changes:
```bash
sudo systemctl restart systemd-journald
```

## Project Layout
- src/: application code (client, queue, aggregation)
- tests/: unit tests for aggregation logic
- CMakeLists.txt: build configuration
- conanfile.txt: dependencies

## Notes
- Streams are configured via config.json/CLI.
- Output is written to a file; stdout mirrors flushed windows when console_report is enabled.
- Aggregator groups trades by their `timestamp` and delays writing by `write_delay_ms` to allow late trades.
- `write_period_ms` controls how often the writer flushes completed windows to disk.

