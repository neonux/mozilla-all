import os

def writetofile(args):
  with open(args[0], 'w') as f:
    f.write(' '.join(args[1:]))

def writeenvtofile(args):
  with open(args[0], 'w') as f:
    f.write(os.environ[args[1]])
