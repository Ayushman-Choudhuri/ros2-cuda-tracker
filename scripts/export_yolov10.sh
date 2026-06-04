#!/usr/bin/env bash
# Downloads YOLOv10 weights, exports to ONNX, builds FP16 TensorRT engine.
# All artifacts are saved to models/.
#
# Requirements:
#   - Python 3 with pip
#   - ultralytics >= 8.2  (auto-installed if missing)
#   - trtexec on PATH (ships with TensorRT; in the devcontainer at /usr/src/tensorrt/bin/trtexec)
#
# Usage:
#   ./scripts/export_yolov10.sh                    # nano (default)
#   ./scripts/export_yolov10.sh --model s          # small
#   ./scripts/export_yolov10.sh --model m          # medium
#   ./scripts/export_yolov10.sh --model l          # large
#   ./scripts/export_yolov10.sh --skip-download    # reuse existing .pt
#   ./scripts/export_yolov10.sh --skip-onnx        # reuse existing .onnx
#   ./scripts/export_yolov10.sh --engine-only      # only build engine, both model files must exist
#   ./scripts/export_yolov10.sh --model s --skip-onnx  # flags compose

set -euo pipefail

# ── Paths ─────────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
MODELS_DIR="$PROJECT_ROOT/models"

# ── Flags ─────────────────────────────────────────────────────────────────────
MODEL_VARIANT="n"
SKIP_DOWNLOAD=false
SKIP_ONNX=false
ENGINE_ONLY=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --model)
            shift
            [[ $# -gt 0 ]] || { echo "ERROR: --model requires an argument (n|s|m|l)"; exit 1; }
            MODEL_VARIANT="$1"
            case "$MODEL_VARIANT" in
                n|s|m|l) ;;
                *) echo "ERROR: --model must be one of: n s m l (got '$MODEL_VARIANT')"; exit 1 ;;
            esac
            ;;
        --skip-download) SKIP_DOWNLOAD=true ;;
        --skip-onnx)     SKIP_ONNX=true ;;
        --engine-only)   ENGINE_ONLY=true; SKIP_DOWNLOAD=true; SKIP_ONNX=true ;;
        *) echo "Unknown flag: $1"; exit 1 ;;
    esac
    shift
done

# ── Derived paths ─────────────────────────────────────────────────────────────
MODEL_NAME="yolov10${MODEL_VARIANT}"
PT_FILE="$MODELS_DIR/${MODEL_NAME}.pt"
ONNX_FILE="$MODELS_DIR/${MODEL_NAME}.onnx"
ENGINE_FILE="$MODELS_DIR/${MODEL_NAME}_fp16.engine"
WEIGHTS_URL="https://github.com/THU-MIG/yolov10/releases/download/v1.1/${MODEL_NAME}.pt"

# ── Helpers ───────────────────────────────────────────────────────────────────
log() { echo "[export_yolov10] $*"; }
die() { echo "[export_yolov10] ERROR: $*" >&2; exit 1; }

require_cmd() {
    command -v "$1" &>/dev/null || die "'$1' not found on PATH. $2"
}

# ── Dependency checks ─────────────────────────────────────────────────────────
require_cmd python3 "Install Python 3."

# trtexec may live under the TensorRT samples bin — add the common devcontainer path
export PATH="$PATH:/usr/src/tensorrt/bin"
require_cmd trtexec \
    "trtexec not found. Install TensorRT or run inside the devcontainer."

mkdir -p "$MODELS_DIR"

log "Model variant: ${MODEL_NAME}"

# ── Step 1: Download weights ───────────────────────────────────────────────────
if ! $SKIP_DOWNLOAD; then
    if [[ -f "$PT_FILE" ]]; then
        log "${MODEL_NAME}.pt already exists — skipping download. Pass --skip-download to suppress this message."
    else
        log "Downloading ${MODEL_NAME}.pt …"
        if command -v wget &>/dev/null; then
            wget -q --show-progress -O "$PT_FILE" "$WEIGHTS_URL"
        elif command -v curl &>/dev/null; then
            curl -L --progress-bar -o "$PT_FILE" "$WEIGHTS_URL"
        else
            die "Neither wget nor curl found. Install one to download weights."
        fi
        log "Saved: $PT_FILE"
    fi
fi

[[ -f "$PT_FILE" ]] || die "${MODEL_NAME}.pt missing at $PT_FILE. Run without --skip-download."

# ── Step 2: Export ONNX ───────────────────────────────────────────────────────
if ! $SKIP_ONNX; then
    log "Checking ultralytics …"
    if ! python3 -c "import ultralytics" &>/dev/null; then
        log "ultralytics not found — installing …"
        python3 -m pip install --quiet "ultralytics>=8.2"
    fi

    ULTRALYTICS_VER=$(python3 -c "import ultralytics; print(ultralytics.__version__)")
    log "ultralytics $ULTRALYTICS_VER"

    log "Exporting ONNX (opset 13, imgsz 640, end2end NMS-free) …"
    python3 - <<PYEOF
from ultralytics import YOLO
import shutil, pathlib

pt = pathlib.Path("$PT_FILE")
model = YOLO(str(pt))

# end2end=True gives the [batch, 300, 6] NMS-free output that the C++ Detector expects.
export_path = model.export(
    format="onnx",
    imgsz=640,
    opset=13,
    simplify=True,
    dynamic=False,
)

src = pathlib.Path(export_path)
dst = pathlib.Path("$ONNX_FILE")
if src != dst:
    shutil.move(str(src), str(dst))
    print(f"Moved {src.name} → {dst}")
else:
    print(f"Exported to {dst}")
PYEOF

    log "Saved: $ONNX_FILE"
fi

[[ -f "$ONNX_FILE" ]] || die "${MODEL_NAME}.onnx missing at $ONNX_FILE. Run without --skip-onnx."

# ── Step 3: Build TensorRT FP16 engine ────────────────────────────────────────
log "Building FP16 TensorRT engine (this takes several minutes) …"

# Print GPU info so it's clear which architecture this engine targets.
if command -v nvidia-smi &>/dev/null; then
    GPU_NAME=$(nvidia-smi --query-gpu=name --format=csv,noheader | head -1)
    log "Target GPU: $GPU_NAME"
    log "Note: this .engine is architecture-specific. Rebuild on Jetson before deploying."
fi

trtexec \
    --onnx="$ONNX_FILE" \
    --fp16 \
    --saveEngine="$ENGINE_FILE" \
    --iterations=10

[[ -f "$ENGINE_FILE" ]] || die "Engine file not written — trtexec failed. Check output above."
log "Saved: $ENGINE_FILE"

# ── Done ──────────────────────────────────────────────────────────────────────
echo ""
log "Done. Artifacts in $MODELS_DIR:"
ls -lh "$MODELS_DIR"/*.pt "$MODELS_DIR"/*.onnx "$MODELS_DIR"/*.engine 2>/dev/null || true
echo ""
log "Reminder: do NOT commit .pt or .engine files. Only ${MODEL_NAME}.onnx goes to git."
