# Repository Benchmark

Compares how MiNiFi C++'s repository implementations behave under a continuous ingest
workload. A Python orchestrator drives a MiNiFi Docker container: it writes a
`minifi.properties` selecting the repositories under test, periodically generates input
files into a mounted directory, and periodically samples three metrics:

- flowfile repository size on disk (`du` inside the container)
- content repository size on disk (`du` inside the container)
- process memory usage (Docker stats API)

Samples are written to a JSON result file, which `generate_report.py` turns into an HTML
report with comparison charts.

The benchmark flow is a simple pass-through: `GetFile` (from `/tmp/input`) →
`LogAttribute` (auto-terminated), defined in `resources/get_config.yml`.

## Requirements

```bash
python -m venv venv && source venv/bin/activate
pip install -r requirements.txt
```

A MiNiFi C++ Docker image is also required. Build one with `docker/DockerBuild.sh`, making
sure the `rocksdb-repos` and `lmdb` extensions are enabled if you want to benchmark those
repositories.

## Running a benchmark

```bash
python main.py \
  --image apacheminificpp:latest \
  --flowfile-repository rocksdb \
  --content-repository filesystem \
  --duration 60 \
  --input-interval 1 \
  --input-file-size 1M \
  --metrics-interval 2
```

### Repository choices

| Flag value   | FlowFile (`--flowfile-repository`) | Content (`--content-repository`) |
|--------------|------------------------------------|----------------------------------|
| `rocksdb`    | `FlowFileRepository`               | `DatabaseContentRepository`      |
| `lmdb`       | `LmdbFlowFileRepository`           | `LmdbContentRepository`          |
| `filesystem` | —                                  | `FileSystemRepository`           |
| `volatile`   | `VolatileFlowFileRepository`       | `VolatileContentRepository`      |

Volatile repositories keep data in memory and have no directory on disk, so their reported
size is always 0 by design.

### Options

- `--duration` total session length in seconds (default 120)
- `--input-interval` seconds between input file generation cycles (default 1)
- `--input-file-size` size of each generated file, e.g. `512K`, `1M`, `1G` (default 1M)
- `--input-files-per-cycle` files generated each cycle (default 1)
- `--metrics-interval` seconds between metric samples (default 5)
- `--output` JSON output path (default `results/<timestamp>_<ff>_<content>.json`)

## Generating a report

Pass one or more result files; each becomes a series in the charts so runs can be compared:

```bash
python generate_report.py results/*.json -o results/report.html
```

Open `results/report.html` in a browser (it loads Chart.js from a CDN).
