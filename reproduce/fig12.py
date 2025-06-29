# fig12.py
# need more test...

import os
import subprocess
import time
import sys

root_path = root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
checker_path = os.path.join(root_path, 'precompiled', 'builddir-release-veristrong', 'checker')
history_path = os.path.join(root_path, 'history', 'fig12')
results = []

setups = [
  {
    'name': 'veristrong',
    'pruning': 'fast',
    'solver': 'acyclic-minisat', 
    'isolation-level': 'ser',
  }, 
  {
    'name': 'veristrong',
    'pruning': 'fast',
    'solver': 'acyclic-minisat', 
    'isolation-level': 'si',
  }, 
]

def run_single(setup, history):
  print(history, file=sys.stderr)
  cmd = [checker_path, history, 
         '--pruning', setup['pruning'], 
         '--solver', setup['solver'],
         '--isolation-level', setup['isolation-level']]  
  st_time = time.time()
  subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
  ed_time = time.time()
  duration = ed_time - st_time
  return duration

def run(setup):
  isolation_path = os.join(history_path, setup['isolation-level'])
  for hist in os.listdir(isolation_path):
    runtimes = [run_single(setup, os.path.join(isolation_path, hist, _, 'history.bincode')) for _ in os.listdir(os.path.join(history_path, hist))]
    avg_runtime = sum(runtimes) / len(runtimes)
    results.append({
      'isolation': setup['isolation-level'], 
      'hist': hist,
      'runtime': avg_runtime
    })
    
for setup in setups:
  print(f"[{setup['name']}]", file=sys.stderr)
  run(setup)

print('isolation,hist,runtime')
for result in results:
  print(f"{result['isolation']},{result['hist']},{result['runtime']}")
