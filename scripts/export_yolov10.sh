#!/usr/bin/env bash
# Downloads YOLOv10 weights, exports to ONNX, builds FP16 TensorRT engine.
# Artifacts are saved to models/pt/, models/onnx/, and models/engine/.
#
# Requirements:
#   - Python 3 with pip
#   - ultralytics >= 8.2  (auto-installed if missing)
#   - trtexec on PATH (ships with TensorRT; in the devcontainer at /usr/src/tensorrt/bin/trtexec)
#
# Usage:
#   ./scripts/export_yolov10.sh                        # nano (default)
#   ./scripts/export_yolov10.sh --model s              # small
#   ./scripts/export_yolov10.sh --model x              # extra-large
#   ./scripts/export_yolov10.sh --model n s l          # multiple variants
#   ./scripts/export_yolov10.sh --all                  # all six: n s m b l x
#   ./scripts/export_yolov10.sh --skip-download        # reuse existing .pt files
#   ./scripts/export_yolov10.sh --skip-onnx            # reuse existing .onnx files
#   ./scripts/export_yolov10.sh --engine-only          # only build engines, all model files must exist
#   ./scripts/export_yolov10.sh --all --skip-onnx      # flags compose

set -euo pipefail

# ── Paths ─────────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
MODELS_DIR="$PROJECT_ROOT/models"
PT_DIR="$MODELS_DIR/pt"
ONNX_DIR="$MODELS_DIR/onnx"
ENGINE_DIR="$MODELS_DIR/engine"

# ── Flags ─────────────────────────────────────────────────────────────────────
MODEL_VARIANTS=()
SKIP_DOWNLOAD=false
SKIP_ONNX=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --model)
            shift
            [[ $# -gt 0 ]] || { echo "ERROR: --model requires at least one argument (n|s|m|l)"; exit 1; }
            _prev_count=${#MODEL_VARIANTS[@]}
            while [[ $# -gt 0 && "$1" != --* ]]; do
                case "$1" in
                    n|s|m|b|l|x) MODEL_VARIANTS+=("$1") ;;
                    *) echo "ERROR: --model variants must be one of: n s m b l x (got '$1')"; exit 1 ;;
                esac
                shift
            done
            [[ ${#MODEL_VARIANTS[@]} -gt $_prev_count ]] || { echo "ERROR: --model requires at least one variant (n|s|m|b|l|x)"; exit 1; }
            continue
            ;;
        --all)          MODEL_VARIANTS=(n s m b l x) ;;
        --skip-download) SKIP_DOWNLOAD=true ;;
        --skip-onnx)     SKIP_ONNX=true ;;
        --engine-only)   SKIP_DOWNLOAD=true; SKIP_ONNX=true ;;
        *) echo "Unknown flag: $1"; exit 1 ;;
    esac
    shift
done

# Default to nano if nothing specified.
[[ ${#MODEL_VARIANTS[@]} -eq 0 ]] && MODEL_VARIANTS=("n")

# Deduplicate while preserving order.
declare -A _seen_variants
_deduped=()
for _v in "${MODEL_VARIANTS[@]}"; do
    if [[ -z "${_seen_variants[$_v]+x}" ]]; then
        _deduped+=("$_v")
        _seen_variants[$_v]=1
    fi
done
MODEL_VARIANTS=("${_deduped[@]}")
unset _seen_variants _deduped _v

# ── Helpers ───────────────────────────────────────────────────────────────────
log() { echo "[export_yolov10] $*"; }
die() { echo "[export_yolov10] ERROR: $*" >&2; exit 1; }

require_cmd() {
    command -v "$1" &>/dev/null || die "'$1' not found on PATH. $2"
}

# ── One-time checks ───────────────────────────────────────────────────────────
require_cmd python3 "Install Python 3."

export PATH="$PATH:/usr/src/tensorrt/bin"
require_cmd trtexec \
    "trtexec not found. Install TensorRT or run inside the devcontainer."

mkdir -p "$PT_DIR" "$ONNX_DIR" "$ENGINE_DIR"

if ! $SKIP_ONNX; then
    log "Checking ultralytics …"
    if ! python3 -c "import ultralytics" &>/dev/null; then
        log "ultralytics not found — installing …"
        python3 -m pip install --quiet "ultralytics>=8.2"
    fi
    ULTRALYTICS_VER=$(python3 -c "import ultralytics; print(ultralytics.__version__)")
    log "ultralytics $ULTRALYTICS_VER"
fi

if command -v nvidia-smi &>/dev/null; then
    GPU_NAME=$(nvidia-smi --query-gpu=name --format=csv,noheader | head -1)
    log "Target GPU: $GPU_NAME"
    log "Note: .engine files are architecture-specific. Rebuild on Jetson before deploying."
fi

log "Variants to export: ${MODEL_VARIANTS[*]}"

# ── Per-variant export ────────────────────────────────────────────────────────
export_variant() {
    local variant="$1"
    local model_name="yolov10${variant}"
    local pt_file="$PT_DIR/${model_name}.pt"
    local onnx_file="$ONNX_DIR/${model_name}.onnx"
    local engine_file="$ENGINE_DIR/${model_name}_fp16.engine"
    local weights_url="https://github.com/THU-MIG/yolov10/releases/download/v1.1/${model_name}.pt"

    log "──────────────────────────────────────────"
    log "Processing: ${model_name}"

    # Step 1: Download weights
    if ! $SKIP_DOWNLOAD; then
        if [[ -f "$pt_file" ]]; then
            log "${model_name}.pt already exists — skipping download."
        else
            log "Downloading ${model_name}.pt …"
            local tmp_pt; tmp_pt="$(mktemp "${pt_file}.XXXXXX")"
            trap 'rm -f "$tmp_pt"' EXIT
            if command -v wget &>/dev/null; then
                wget -q --show-progress -O "$tmp_pt" "$weights_url"
            elif command -v curl &>/dev/null; then
                curl -L --fail --progress-bar -o "$tmp_pt" "$weights_url"
            else
                rm -f "$tmp_pt"
                die "Neither wget nor curl found. Install one to download weights."
            fi
            mv "$tmp_pt" "$pt_file"
            trap - EXIT
            log "Saved: $pt_file"
        fi
    fi

    [[ -f "$pt_file" ]] || die "${model_name}.pt missing at $pt_file. Run without --skip-download."

    # Step 2: Export ONNX
    if ! $SKIP_ONNX; then
        log "Exporting ONNX (opset 13, imgsz 640, end2end NMS-free) …"
        EXPORT_PT_FILE="$pt_file" EXPORT_ONNX_FILE="$onnx_file" python3 - <<'PYEOF'
import os, shutil, pathlib
from ultralytics import YOLO

pt = pathlib.Path(os.environ["EXPORT_PT_FILE"])
dst = pathlib.Path(os.environ["EXPORT_ONNX_FILE"])
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
if src != dst:
    shutil.move(str(src), str(dst))
    print(f"Moved {src.name} → {dst}")
else:
    print(f"Exported to {dst}")
PYEOF
        log "Saved: $onnx_file"
    fi

    [[ -f "$onnx_file" ]] || die "${model_name}.onnx missing at $onnx_file. Run without --skip-onnx."

    # Step 3: Build TensorRT FP16 engine
    log "Building FP16 TensorRT engine (this takes several minutes) …"
    trtexec \
        --onnx="$onnx_file" \
        --fp16 \
        --saveEngine="$engine_file"

    [[ -s "$engine_file" ]] || die "Engine file missing or empty — trtexec failed. Check output above."
    log "Saved: $engine_file"
}

for variant in "${MODEL_VARIANTS[@]}"; do
    export_variant "$variant"
done

# ── Done ──────────────────────────────────────────────────────────────────────
echo ""
log "Done. Artifacts:"
ls -lh "$PT_DIR"/*.pt "$ONNX_DIR"/*.onnx "$ENGINE_DIR"/*.engine 2>/dev/null || true
echo ""
log "Reminder: do NOT commit model artifacts — models/ is gitignored."
