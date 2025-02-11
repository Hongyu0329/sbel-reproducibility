#!/usr/bin/env bash
##SBATCH --gres=gpu:titanrtx:1
#SBATCH --time=10-0:0:0
#SBATCH --partition=sbel
#SBATCH --account=sbel
#SBATCH --qos=sbel_owner
#SBATCH --cpus-per-task=30

#SBATCH -w euler44

##module load gcc/7.3.0
##module load cmake/3.15.4
##module load cuda/10.1

python3 granular_gen.py
