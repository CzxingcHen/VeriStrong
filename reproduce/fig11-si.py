# fig11-si.py

import os
import subprocess
import time
import sys

root_path = root_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
history_path = os.path.join(root_path, 'history', 'fig11', 'si')
results = []

setups = [
  {
    'name': 'veristrong',
    'checker': os.path.join(root_path, 'precompiled', 'builddir-release-veristrong', 'checker'),
    'pruning': 'fast',
    'solver': 'acyclic-minisat', 
    'isolation-level': 'si',
    'history-type': 'viper',
  }, 
  {
    'name': 'polysi',
    'checker': os.path.join(root_path, 'tools', 'PolySI', 'build', 'libs', 'PolySI-1.0.0-SNAPSHOT.jar'),
    'history-type': 'viper',
  }, 
  {
    'name': 'viper',
    'checker': os.path.join(root_path, 'tools', 'Viper'),
    'config': os.path.join(root_path, 'tools', 'Viper', 'src', 'config.tmp.yaml'),
    'history-type': 'viper',
  }, 
  {
    'name': 'dbcop',
    'checker': os.path.join(root_path, 'tools', 'dbcop-verifier', 'target', 'release', 'dbcop'),
    'history-type': 'dbcop',
    'isolation-level': 'si'
  }, 
]

def run_single(setup, history):
  # here the history-type is hard encoded...
  print(history, file=sys.stderr)
  if setup['name'] == 'veristrong':
    cmd = [setup['checker'], os.path.join(history, 'log'), 
          '--pruning', setup['pruning'], 
          '--solver', setup['solver'],
          '--isolation-level', setup['isolation-level'], 
          '--history-type', 'cobra-uv']  
  elif setup['name'] == 'polysi':
    polysi_path = setup['checker']
    cmd = ['java', '-jar', polysi_path, 'audit', '--type=cobra', os.path.join(history, 'log')]  
  elif setup['name'] == 'viper':
    viper_path = os.path.join(setup['checker'], 'src', 'main_allcases.py')
    config_path = setup['config']
    cmd = ['python3', viper_path, 
           '--config_file', config_path, 
           '--algo', '6', 
           '--sub_dir', os.path.join(history, 'json'),
           '--perf_file', '/tmp/test_perf.txt',
           '--exp_name', 'test', 
           '--strong-session']
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
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=60)
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
