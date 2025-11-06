# ZERO: Zero Entropy Region Optimization for Data Deduplication

This is the implementation repository of our FAST'25 paper: **ZERO: Zero Entropy Region Optimization for Data Deduplication**.

## Description
As the volume of digital data continues to grow rapidly, efficient data reduction techniques have become essential for managing storage and bandwidth. Recent research has shown that low-entropy and zero-entropy data regions are not handled efficiently by data deduplication, a widely used data reduction technique. We characterize the zero-entropy data regions present in real-world datasets. Using these insights, we design **ZERO** to complement traditional deduplication, effectively handling zero-entropy regions. Our evaluation shows that ZERO can improve the space savings achieved by dedupli-
cation systems by up to 29% in the presence of zero-entropy regions, while imposing minimal processing time overhead.

This artifact is based on **DedupBench** [Liu et al., CCECE 2023](https://doi.org/10.1109/CCECE58730.2023.10288834), a benchmarking framework for evaluating data chunking techniques used in data deduplication systems. We extend DedupBench to incorporate ZERO’s preprocessing workflow and to evaluate the interaction between ZERO and state-of-the-art CDC algorithms. Specifically, this artifact focuses on the following four CDC techniques: AE, FastCDC, Rabin, and RAM.

# Environment
All experiments were conducted on a [CloudLab](https://www.cloudlab.us/) node (c6525-25g) provisioned with an AMD EPYC 7302P processor (16 cores) running at 3.0 GHz, and 128 GB of RAM. The node used a UBUNTU22-64-X86 (Ubuntu 22.04 LTS) boot image in normal operation mode. No acceleration features were enabled during experiments to ensure consistent and reproducible benchmarking across all CDC algorithms.

# Installation 
1. Install prerequisites. Note that these commands are for Ubuntu 22.04.
   ```
     sudo apt update
     sudo apt install libssl-dev
     sudo apt install python3
     sudo apt install python3-pip
     python3 -m pip install matplotlib
     python3 -m pip install seaborn
   ```
2. Clone and build the repository.
   ```
     git clone https://github.com/MumenJarrah/ZERO_2.git
     cd ZERO/build/
     make clean
     make
   ```
4. For the datasets, download and use the following datasets: [Kaggle](https://www.kaggle.com/datasets/sreeharshau/vm-deb-fast25).

# Running dedup-bench
This section describes how to run dedup-bench. You can run dedup-bench using our preconfigured scripts for 8KB chunks or manually if you want custom techniques/chunk sizes.

## Preconfigured Run - 8 KB chunks
We have created scripts to run dedup-bench with an 8KB average chunk size on any given dataset. These commands run all the CDC techniques shown in the VectorCDC paper from FAST 2025. 
1. Go into the dedup-bench build directory.
```
  cd <dedup_bench_root_dir>/build/
```
2. Run dedup-script with your chosen dataset. Replace `<path_to_dataset>` with the directory of the random dataset you previously created / any other dataset of your choice. **_Note that VRAM-512 will not run when compiled without AVX-512 support_**.
```
  ./dedup_script.sh -t 8kb_fast25 <path_to_dataset>
```
3. Plot a graph with the throughput results from all CDC algorithms (including VRAM) on your dataset. The graph is saved in `results_graph.png`.
```
  python3 plot_throughput_graph.py results.txt
```


## Manual Runs - Custom techniques/chunk sizes
1. Choose the required chunking, hashing techniques, and chunk sizes by modifying `config.txt`. The default configuration runs SeqCDC with an average chunk size of 8 KB. Supported parameter values are given in the next section and sample config files are available in `build/config_8kb_fast25/`.
   ```
     cd <dedup_bench_repo_dir>/build/
     vim config.txt
   ```
2. Run dedup-bench. Note that the path to be passed is a directory and that the output is generated in a file `hash.out`. Throughput and avg chunk size are printed to stdout.
   ```
     ./dedup.exe <path_to_random_dataset_dir> config.txt
   ```
3. Measure space savings. Note that space savings will be zero if the random dataset is used.
   ```
     ./measure-dedup.exe hash.out
   ```

# Supported Chunking and Hashing Techniques

Here are some hints using which `config.txt` can be modified.

### Chunking Techniques
WE use the following chunking techniques supported by DedupBench. Note that the `chunking_algo` parameter in the configuration file needs to be edited to switch techniques.

| Chunking Technique | chunking_algo |
|--------------------|---------------|
| AE                 | ae            |
| FastCDC            | fastcdc       |
| Rabin's Chunking   | rabins        |
| RAM                | ram           |

After choosing a `chunking_algo`, make sure to check and adjust its parameters (e.g. chunk sizes). _Note that each `chunking_algo` has a separate parameter section in the config file_. For example, SeqCDC's minimum and maximum chunk sizes are called `seq_min_block_size` and `seq_max_block_size` respectively.

### Hashing Techniques
The following hashing techniques are currently supported by DedupBench. Note that the `hashing_algo` parameter in the configuration file needs to be edited to switch techniques.

| Hashing Technique | hashing_algo |
|-------------------|--------------|
| MD5               | md5          |
| SHA1              | sha1         |
| SHA256            | sha256       |
| SHA512            | sha512       |

### Datasets
1. CORD-19: https://www.kaggle.com/datasets/allen-institute-for-ai/CORD-19-research-challenge
2. VDI:
3. LNX: 
