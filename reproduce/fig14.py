# fig14.py

import os
import subprocess
import time
import sys

root_path = root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
history_path = os.path.join(root_path, 'history', 'fig13-14-15-table1')
results = []

setups = [
  {
    'name': 'veristrong',
    'pruning': 'fast',
    'checker': 'builddir-release-veristrong',
  }, 
  {
    'name': 'veristrong-no-H',
    'pruning': 'fast',
    'checker': 'builddir-release-no-H',
  }, 
  {
    'name': 'veristrong-no-HC',
    'pruning': 'fast',
    'checker': 'builddir-release-no-HC',
  }, 
  {
    'name': 'veristrong-no-HCP',
    'pruning': 'none',
    'checker': 'builddir-release-no-HC',
  }, 
]

def run_single(setup, history_type, history):
  checker_path = os.path.join(root_path, 'precompiled', setup['checker'], 'checker')
  print(history, file=sys.stderr)
  cmd = [checker_path, history, 
         '--pruning', setup['pruning'], 
         '--history-type', history_type]  
  st_time = time.time()
  try:
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=600)
  except subprocess.TimeoutExpired as e:
    return 'TO'
  ed_time = time.time()
  duration = ed_time - st_time
  return duration

def run(setup):
  for hist in ['tpcc-10k', 'rubis-10k', 'twitter-10k', 'RH-10k', 'BL-10k', 'WH-10k', 'HD-10k']:
    history_type = 'dbcop' if hist in ['RH-10k', 'BL-10k', 'WH-10k', 'HD-10k'] else 'cobra'
    runtime = run_single(setup, history_type, os.path.join(history_path, hist, 'hist-00000', 'history.bincode'))
    results.append({
      'tool': setup['name'], 
      'hist': hist,
      'runtime': runtime
    })
    
for setup in setups:
  print(f"[{setup['name']}]", file=sys.stderr)
  run(setup)

print('tool,hist,runtime')
for result in results:
  print(f"{result['tool']},{result['hist']},{result['runtime']}")
