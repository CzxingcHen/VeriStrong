# fig11-ser.py

import os
import subprocess
import time
import sys
import psutil

root_path = root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
history_path = os.path.join(root_path, 'history', 'fig11', 'ser')
results = []

setups = [
  {
    'name': 'veristrong',
    'checker': os.path.join(root_path, 'precompiled', 'builddir-release-veristrong', 'checker'),
    'pruning': 'fast',
    'solver': 'acyclic-minisat', 
    'isolation-level': 'ser',
    'history-type': 'dbcop',
  }, 
  {
    'name': 'cobra',
    'checker': os.path.join(root_path, 'tools', 'CobraVerifier'),
    'config': os.path.join(root_path, 'tools', 'CobraVerifier', 'cobra.conf.nogpu'),
    'history-type': 'cobra',
  }, 
  {
    'name': 'dbcop',
    'checker': os.path.join(root_path, 'tools', 'dbcop-verifier', 'target', 'release', 'dbcop'),
    'history-type': 'dbcop',
    'isolation-level': 'ser'
  }, 
]

def run_single(setup, history):
  print(history, file=sys.stderr)
  if setup['name'] == 'veristrong':
    cmd = [setup['checker'], os.path.join(history, 'history.bincode'), 
          '--pruning', setup['pruning'], 
          '--solver', setup['solver'],
          '--isolation-level', setup['isolation-level']]  
  elif setup['name'] == 'cobra':
    cobra_path = setup['checker']
    config_path = setup['config']
    cmd = ['java',
           f'-Djava.library.path={cobra_path}/include/:{cobra_path}/build/monosat',
           '-jar', f"{cobra_path}/target/CobraVerifier-0.0.1-SNAPSHOT-jar-with-dependencies.jar", 
           'mono', 'audit', config_path, history]  
  elif setup['name'] == 'dbcop':
    dbcop_path = setup['checker']
    cmd = [dbcop_path, 'verify', 
                       '-c', setup['isolation-level'],
                       '--out_dir', '/tmp/dbcop_output',
                       '--ver_dir', history]
  else:
    assert False
  
  st_time = time.time()
  try:
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
  except subprocess.TimeoutExpired as e:
    return 'TO'
  ed_time = time.time()
  duration = ed_time - st_time
  return duration

def run(setup):
  isolation_path = os.path.join(history_path, setup['history-type'])
  for hist in os.listdir(isolation_path):
    history = os.path.join(isolation_path, hist) if setup['history-type'] == 'dbcop' else os.path.join(isolation_path, hist)
    runtime = run_single(setup, history) 
    results.append({
      'tool': setup['name'], 
      'hist': hist,
      'runtime': runtime,
    })
    
for setup in setups:
  print(f"[{setup['name']}]", file=sys.stderr)
  run(setup)

print('tool,hist,runtime')
for result in results:
  print(f"{result['tool']},{result['hist']},{result['runtime']}")
