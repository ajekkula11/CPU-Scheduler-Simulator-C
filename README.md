# Assignment 4: Neural Style Transfer

**Course:** CSCI611  
**Topic:** Image Style Transfer Using Convolutional Neural Networks (Gatys et al., CVPR 2016)

---

## Overview

This assignment implements neural style transfer, which synthesises a new image that preserves the *content* of one photograph while adopting the *artistic style* of another. The approach uses a pre-trained VGG19 network as a fixed feature extractor and optimises directly on the output image pixels via gradient descent.

**Content image:** Tübingen, Germany (400×532 px)  
**Style image:** *The Starry Night* by Vincent van Gogh (1889)

---

## Repository Structure

```
Assignment_4/
├── Style_Transfer.ipynb       # Main Jupyter/Colab notebook with implementation + report
├── README.md                  # This file
├── report.pdf                 # PDF report summarising implementation and findings
├── images/
│   ├── tubingen.jpg           # Content image
│   └── starry_night.jpg       # Style image
└── outputs/                   # Generated output images from experiments
```

---

## Requirements

The notebook is designed to run on **Google Colab** (recommended) with GPU acceleration enabled.

**Dependencies** (all pre-installed on Colab):

| Package | Purpose |
|---|---|
| `torch` / `torchvision` | Deep learning framework & VGG19 model |
| `PIL` (Pillow) | Image loading and saving |
| `matplotlib` | Visualisation |
| `numpy` | Numerical operations |

---

## How to Run

### Option 1 — Google Colab (Recommended)

1. Open [Google Colab](https://colab.research.google.com/)
2. Upload `Style_Transfer.ipynb` via **File → Upload notebook**
3. Enable GPU: **Runtime → Change runtime type → T4 GPU → Save**
4. Upload the content and style images when prompted (or mount Google Drive)
5. Run all cells: **Runtime → Run all**

### Option 2 — Local Jupyter

1. Clone or download this repository
2. Install dependencies:
   ```bash
   pip install torch torchvision pillow matplotlib numpy
   ```
3. Launch Jupyter:
   ```bash
   jupyter notebook Style_Transfer.ipynb
   ```
4. Run all cells top to bottom

> **Note:** A CUDA-capable GPU is strongly recommended. CPU execution is possible but will be significantly slower (expect 30–60× longer per run).

---

## Implementation Summary

The notebook completes three TODOs from the exercise:

**TODO 1 — `get_features()`:** Maps VGG19 layer indices to named layers used for content and style extraction.

```python
layers = {
    '0' : 'conv1_1',   # style
    '5' : 'conv2_1',   # style
    '10': 'conv3_1',   # style
    '19': 'conv4_1',   # style
    '21': 'conv4_2',   # content
    '28': 'conv5_1',   # style
}
```

**TODO 2 — `gram_matrix()`:** Computes inter-channel feature correlations (position-invariant texture representation).

```python
def gram_matrix(tensor):
    batch_size, d, h, w = tensor.size()
    tensor = tensor.view(batch_size * d, h * w)
    gram   = torch.mm(tensor, tensor.t())
    return gram
```

**TODO 3 — Training loop:** Computes content loss (MSE at `conv4_2`), style loss (Gram matrix MSE across 5 layers), and total loss; updates pixel values via Adam optimiser.

---

## Experiments

Five hyperparameter experiments are conducted and documented in the notebook and report:

| Experiment | Variable | Values Tested |
|---|---|---|
| 1 | Style weight β | 1e4, 1e6 (default), 1e8 |
| 2 | Per-layer style weights | Early-biased, uniform, late-biased |
| 3 | Learning rate | 0.0005, 0.003 (default), 0.01 |
| 4 | Number of steps | 500, 1000, 2000 (default), 4000 |
| 5 | Target initialisation | Content image vs. random noise |

Key findings are summarised in `report.pdf`.

---

## Baseline Configuration

| Hyperparameter | Value |
|---|---|
| Content weight α | 1 |
| Style weight β | 1e6 |
| Learning rate | 0.003 (Adam) |
| Steps | 2000 |
| Initialisation | Content image |
| Hardware | CUDA GPU |

Baseline loss converged from ~1.0×10⁷ at step 400 to ~8.9×10⁵ at step 2000 (~91% reduction).

---

## References

- Gatys, L. A., Ecker, A. S., & Bethge, M. (2016). *Image Style Transfer Using Convolutional Neural Networks*. CVPR 2016.
- Simonyan, K., & Zisserman, A. (2014). *Very Deep Convolutional Networks for Large-Scale Image Recognition*. arXiv:1409.1556.
- [PyTorch VGG Documentation](https://pytorch.org/vision/stable/models/vgg.html)
