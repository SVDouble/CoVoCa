# covoca-toolkit

Python helper package for Gaussian Splatting data preparation and analysis.

Run from the repository root:

```bash
uv run --project covoca_toolkit --python 3.14 download-aruco-sample
```

Build source and wheel distributions with `uv`:

```bash
uv build --project covoca_toolkit
```

Optional Plotly dependencies are declared as the `plot` extra:

```bash
uv run --project covoca_toolkit --extra plot --python 3.14 python
```
