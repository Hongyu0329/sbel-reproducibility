# plot convergence result of slider crank and four link w.r.t kinematic results loaded from pickle
# the reference solution are generated from cmp_w_kinematics.py
# system constrained kinematically, so velocity is used here

import pickle
import time
import warnings
import os
import matplotlib.pyplot as plt
from matplotlib import ticker
import numpy as np
import platform

# check operating system, file loc different
if platform.system() == 'Darwin':
    fileloc = "/Users/luning/Manuscripts/Conference/2022/IROS_Half_Implicit/images/"
if platform.system() == 'Linux':
    fileloc = "/home/luning/Manuscripts/Journal/2022/HalfImplicit/images/"
        


# flag for saving pic directly in image folder
saveFig = True

dir_path = './ground_truth/'

# load picked kinematics solution 
model_name ="double_pendulum"
body = 1
component = 2
# body = 2
# component = 0




filename = "{}_accuracy_analysis_updated.png".format(model_name)

dt_ref = 1e-6 # resolution of ode solver

pickle_name = '{}_ode_soln.pickle'.format(model_name)
with open(dir_path + pickle_name, 'rb') as handle:
    
    
    pos_exact, dt_exact, t_end = pickle.load(handle)
        
    
forms = ['rA_half', 'rA']
line_styles = {"rA_half": "b-o", "rA": "r-o"}

labels = {"rA_half": "HI", "rA": "FI"}
colors = {"rA_half": "blue", "rA": "red"}

step_sizes = [1e-4, 2e-4, 4e-4, 1e-3, 2e-3, 4e-3, 1e-2, 2e-2, 4e-2]

num_bodies = 2

fig, ax = plt.subplots(1,1,figsize=(45, 30))

Fontsize = 120
LineWidth = 10
MarkerSize = 40
plt.rc('font', size=Fontsize)
plt.rc('legend', fontsize=Fontsize*1.4)
plt.rc('figure', titlesize=Fontsize)
plt.rc('lines', linewidth=LineWidth)
plt.rc('lines', markersize=MarkerSize)
plt.rc('axes', linewidth=LineWidth*0.5)


for form in forms:
    
    diff_list = []

    for step_size in step_sizes:
        
        pickle_name = '{}_{}_dynamics_dt_{}.pickle'.format(model_name, form, step_size)
        
        # load picked solution from dynamics solver
    
        with open(dir_path + pickle_name, 'rb') as handle:
            _, pos, numItr = pickle.load(handle)
                    
        t = np.arange(0, t_end, step_size)
        pos_ground_truth = np.full((num_bodies, 3, len(t)), np.nan)
    
        # exact solution is finer, make sure the length is the same for comparison
        for i in range(0, len(t)):
            pos_ground_truth[:, :, i] = pos_exact[:, :, round(step_size/dt_ref)*i]
            
        diff = np.abs(pos_ground_truth[body, component, :] - pos[body, component, :])
        diff_list.append(np.sqrt(np.sum(np.power(diff,2)))/np.size(diff))
        # diff_list.append(diff)
    
    ax.loglog(step_sizes, np.array(diff_list), line_styles[form], label=labels[form])

ax.legend()
ax.grid(b=True, which='major', linestyle='-', linewidth=LineWidth*0.5)
ax.grid(b=True, which='minor', linestyle='--', linewidth=LineWidth*0.5)
ax.minorticks_on()
ax.set_xlabel('step size', fontsize = Fontsize*1)
ax.set_ylabel('{} pos-{} diff (m/s)'.format('bar 2', 'z'), fontsize = Fontsize*1)
pretty_name = " ".join(model_name.split('_'))
ax.set_title('{} convergence analysis'.format(pretty_name))


if saveFig == True:
    plt.savefig(fileloc + filename)