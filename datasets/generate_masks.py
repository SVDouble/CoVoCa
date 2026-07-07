#!/usr/bin/env python3.14
# /// script
# requires-python = ">=3.14"
# dependencies = [
#   "numpy==2.4.6",
#   "pillow==12.2.0",
#   "timm==1.0.27",
#   "torch==2.12.0",
#   "torchvision==0.27.0",
#   "transformers==5.12.1",
#   "ultralytics==8.4.66",
# ]
# ///
"""Generate foreground masks for the local object datasets.

Grounding DINO finds one prompt-guided object box, then SAM 2.1 Large turns
that box into a binary foreground mask. Each run writes to a new datetime-named
folder and never replaces existing masks. Model weights are downloaded by the
Python model libraries on first use.

Usage:
    uv run --script --python 3.14 datasets/generate_masks.py --dataset cat
    uv run --script --python 3.14 datasets/generate_masks.py --device cpu
"""

import argparse
from collections import Counter
from datetime import datetime
from pathlib import Path

import numpy as np
from PIL import Image

DATASETS_ROOT = Path("local/datasets")
OUTPUT_ROOT = Path("local/annotations/segmentation_masks")
SAM_MODEL = Path("local/models/sam2.1_l.pt")
DETECTOR_MODEL = "IDEA-Research/grounding-dino-tiny"
IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".webp"}
DEFAULT_PROMPTS = {
    "bottle": "a transparent water bottle with a blue cap",
    "car": "a small red toy car",
    "case": "a black hard carrying case with an orange zipper",
    "cat": "a small wooden cat figurine",
    "chair": "a small red miniature chair",
    "cone": "a red traffic cone",
    "cube": "a Rubik cube",
    "cup": "a red cup",
    "helmet": "a small red toy helmet",
    "mushroom": "a mushroom figurine",
    "remote": "a remote control",
    "salt": "a red rectangular salt box",
    "shoe-cup": "a red shoe shaped cup",
    "shuttlecock": "a shuttlecock",
    "spoon": "a spoon",
    "tuber": "an orange climbing belay device",
    "wineglass": "a transparent wine glass",
}

type Job = tuple[str, str, Path, str]


class GroundedSegmenter:
    """Grounding DINO box detector plus SAM mask predictor."""

    def __init__(self, detector_name: str, sam_model: Path, device: str) -> None:
        import torch
        from transformers import AutoModelForZeroShotObjectDetection, AutoProcessor
        from ultralytics import SAM

        if device == "cuda" and not torch.cuda.is_available():
            raise RuntimeError(
                "CUDA was requested but PyTorch cannot see a CUDA device"
            )
        sam_model.parent.mkdir(parents=True, exist_ok=True)

        self.torch = torch
        self.device = device
        self.sam_device = "cuda:0" if device == "cuda" else "cpu"
        self.processor = AutoProcessor.from_pretrained(detector_name)
        self.detector = (
            AutoModelForZeroShotObjectDetection.from_pretrained(detector_name)
            .to(device)
            .eval()
        )
        self.segmenter = SAM(str(sam_model))

    def detect(
        self, image: Image.Image, prompt: str, min_score: float, max_box_area: float
    ) -> tuple[list[float], float]:
        inputs = self.processor(
            images=image, text=f"{prompt}.", return_tensors="pt"
        ).to(self.device)
        with self.torch.no_grad():
            outputs = self.detector(**inputs)
        result = self.processor.post_process_grounded_object_detection(
            outputs,
            inputs.input_ids,
            threshold=0.05,
            text_threshold=0.05,
            target_sizes=[image.size[::-1]],
        )[0]

        boxes = result["boxes"]
        scores = result["scores"]
        if len(scores) == 0:
            raise ValueError(f"No detection for prompt {prompt!r}")
        box_areas = (
            (boxes[:, 2] - boxes[:, 0])
            * (boxes[:, 3] - boxes[:, 1])
            / (image.width * image.height)
        )
        valid = self.torch.where(box_areas <= max_box_area)[0]
        if len(valid) == 0:
            raise ValueError(
                f"No detection smaller than {max_box_area:.0%} for prompt {prompt!r}"
            )
        best = valid[self.torch.argmax(scores[valid])]
        score = float(scores[best])
        if score < min_score:
            raise ValueError(
                f"Detection confidence {score:.3f} is below {min_score:.3f} for prompt {prompt!r}"
            )
        return boxes[best].detach().cpu().tolist(), score

    def segment(self, image_path: Path, box: list[float]) -> Image.Image:
        result = self.segmenter(
            str(image_path), bboxes=[box], device=self.sam_device, verbose=False
        )[0]
        if result.masks is None:
            raise ValueError(f"SAM returned no mask: {image_path}")
        masks = result.masks.data.detach().cpu().numpy() > 0.5
        mask = masks.any(axis=0) if masks.ndim == 3 else masks
        return Image.fromarray(np.uint8(mask) * 255)


def prompt_map(overrides: list[str] | None) -> dict[str, str]:
    prompts = DEFAULT_PROMPTS.copy()
    for value in overrides or []:
        dataset, separator, prompt = value.partition("=")
        if not separator or not dataset or not prompt:
            raise ValueError("--prompt must have the form DATASET=TEXT")
        prompts[dataset] = prompt
    return prompts


def discover_jobs(
    root: Path, selected: set[str] | None, excluded: set[str], prompts: dict[str, str]
) -> list[Job]:
    jobs: list[Job] = []
    for dataset in sorted(root.iterdir(), key=lambda path: path.name.casefold()):
        images_dir = dataset / "images"
        if (
            not images_dir.is_dir()
            or dataset.name in excluded
            or (selected and dataset.name not in selected)
        ):
            continue
        if dataset.name not in prompts:
            raise ValueError(f"No text prompt configured for dataset {dataset.name!r}")
        for image_path in sorted(
            images_dir.iterdir(), key=lambda path: path.name.casefold()
        ):
            if (
                image_path.is_file()
                and image_path.suffix.casefold() in IMAGE_EXTENSIONS
            ):
                jobs.append(
                    (
                        dataset.name,
                        prompts[dataset.name],
                        image_path,
                        f"{image_path.stem}.png",
                    )
                )

    outputs = Counter((dataset, mask_name) for dataset, _, _, mask_name in jobs)
    duplicates = [str(path) for path, count in outputs.items() if count > 1]
    if duplicates:
        raise ValueError(
            f"Image stems produce duplicate masks: {', '.join(duplicates)}"
        )
    return jobs


def validate(mask: Image.Image, image: Image.Image, source: Path) -> float:
    if mask.size != image.size:
        raise ValueError(f"Mask size differs from source image: {source}")
    area = np.count_nonzero(np.asarray(mask)) / (mask.width * mask.height)
    if not 0.0001 < area < 0.50:
        raise ValueError(f"Implausible foreground area {area:.4f}: {source}")
    return area


def create_run_dir(output_root: Path) -> Path:
    run_dir = output_root / datetime.now().strftime("%Y%m%d_%H%M%S")
    run_dir.mkdir(parents=True, exist_ok=False)
    return run_dir


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--datasets-root", type=Path, default=DATASETS_ROOT)
    parser.add_argument("--output-root", type=Path, default=OUTPUT_ROOT)
    parser.add_argument("--sam-model", type=Path, default=SAM_MODEL)
    parser.add_argument("--detector", default=DETECTOR_MODEL)
    parser.add_argument("--device", choices=("cuda", "cpu"), default="cuda")
    parser.add_argument("--min-score", type=float, default=0.25)
    parser.add_argument("--max-box-area", type=float, default=0.30)
    parser.add_argument(
        "--dataset",
        action="append",
        help="process only this dataset; omit to process all datasets",
    )
    parser.add_argument("--exclude", action="append", default=[])
    parser.add_argument(
        "--prompt", action="append", help="override or add a prompt as DATASET=TEXT"
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not 0 < args.min_score < 1 or not 0 < args.max_box_area < 1:
        raise ValueError("Score and area limits must be between zero and one")

    jobs = discover_jobs(
        args.datasets_root,
        set(args.dataset) if args.dataset else None,
        set(args.exclude),
        prompt_map(args.prompt),
    )
    if not jobs:
        raise FileNotFoundError("No dataset images matched the requested selection")

    model = GroundedSegmenter(args.detector, args.sam_model, args.device)
    stats: dict[str, list[tuple[float, float]]] = {}
    run_dir = create_run_dir(args.output_root)
    print(f"Writing masks to {run_dir}")
    for index, (dataset, prompt, source, mask_name) in enumerate(jobs, 1):
        with Image.open(source) as image:
            rgb = image.convert("RGB")
            box, score = model.detect(rgb, prompt, args.min_score, args.max_box_area)
            mask = model.segment(source, box)
            area = validate(mask, image, source)
        target = run_dir / dataset / "masks" / mask_name
        target.parent.mkdir(parents=True, exist_ok=True)
        mask.save(target, optimize=True)
        stats.setdefault(dataset, []).append((score, area))
        print(
            f"[{index}/{len(jobs)}] {dataset}/{source.name} score={score:.3f} area={area:.1%}",
            flush=True,
        )

    for dataset, values in stats.items():
        scores, areas = zip(*values, strict=True)
        print(
            f"{dataset}: {len(values)} masks, score {min(scores):.3f}-{max(scores):.3f}, "
            f"area {min(areas):.1%}-{max(areas):.1%}"
        )
    print(f"Generated {len(jobs)} masks with Grounding DINO and SAM 2.1 Large")
    print(f"Output: {run_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
