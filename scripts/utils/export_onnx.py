#!/usr/bin/env python3
import os
import shutil
import pathlib
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
