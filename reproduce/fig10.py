# fig10.py

import os
import subprocess
import time
import sys

root_path = root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
checker_path = os.path.join(root_path, 'precompiled', 'builddir-release-veristrong', 'checker')
history_path = os.path.join(root_path, 'history', 'fig10')
results = []

setups = [
  {
    'name': 'veristrong',
    'pruning': 'fast',
    'solver': 'acyclic-minisat', 
  }, 
  {
    'name': 'baseline+P', 
    'pruning': 'basic', 
    'solver': 'monosat-baseline',
  }, 
  {
    'name': 'baseline',
    'pruning': 'none', 
    'solver': 'monosat-baseline',
  }
]

def run_single(setup, history):
  print(history, file=sys.stderr)
  cmd = [checker_path, history, 
         '--pruning', setup['pruning'], 
         '--solver', setup['solver']]  
  st_time = time.time()
  try:
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=60)
  except subprocess.TimeoutExpired as e:
    return 'TO'
  ed_time = time.time()
  duration = ed_time - st_time
  return duration

def run(setup):
  for hist in os.listdir(history_path):
    runtimes = [run_single(setup, os.path.join(history_path, hist, _, 'history.bincode')) for _ in os.listdir(os.path.join(history_path, hist))]
    if 'TO' in runtimes:
      avg_runtime = 'TO'
    else: 
      avg_runtime = sum(runtimes) / len(runtimes)
    results.append({
      'tool': setup['name'], 
      'hist': hist,
      'runtime': avg_runtime
    })
    
for setup in setups:
  print(f"[{setup['name']}]", file=sys.stderr)
  run(setup)

print('tool,hist,runtime')
for result in results:
  print(f"{result['tool']},{result['hist']},{result['runtime']}")
