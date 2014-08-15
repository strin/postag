import numpy as np
import os, sys
import numpy.random as npr
import matplotlib.pyplot as plt
import matplotlib as mpl
import itertools
mpl.use('Agg')
from stat_policy import *

def plot(path_l, legend_l, output, color_l=['r','g','b','k'], \
        marker_l=['-','-','-','s']):
  plot_l = list()
  policy_l = list()
  for (pathi, path) in enumerate(path_l):
    time = list()
    acc = list()
    policy_l.append(list())
    for p in path:
      print p
      policy = PolicyResult(p)
      policy_l[-1].append(policy)
      time.append(policy.ave_time())
      acc.append(policy.accuracy)
    p, = plt.plot(time, acc, '%s%s' % (color_l[pathi], marker_l[pathi]))
    plot_l.append(p)
    [time, acc] = zip(*sorted(zip(time,acc), key=lambda ta : ta[0]))
    print time, acc
    (time, acc) = (list(time), list(acc))
    plt.plot(time, acc, '%s-' % (color_l[pathi]))
  plt.legend(plot_l, legend_l, loc=4)
  plt.savefig(output)
  return policy_l
  
if __name__ == '__main__':
  name = sys.argv[1]
  if name == 'wsj':
    if len(sys.argv) >= 3:
      path_in = sys.argv[2]
    else:
      path_in = '.'
    if len(sys.argv) >= 4:
      path_out = sys.argv[3]
    else:
      path_out = '.'
    path_l = list()
    path_l.append(list())
    for T in [1,2,3,4]:
      path_l[-1].append(path_in+'/test_policy/wsj_gibbs_T%d' % T)
    path_l.append(list())
    for thres in [0.5,1.0,1.5,2.0,2.5,3.0]:
      path_l[-1].append(path_in+'/test_policy/wsj_entropy_%0.2f' % thres)
    policy_l = plot(path_l, ['Gibbs', 'Entropy'], path_out+'/wsj.png')
    name_l = [[p.split('/')[-1] for p in path] for path in path_l]
    html = open(path_out+'/wsj.html', 'w')
    html.write(PolicyResult.viscomp(list(itertools.chain(*policy_l)), \
                      list(itertools.chain(*name_l)), ))
  elif name == 'ner':
    if len(sys.argv) >= 3:
      path_in = sys.argv[2]
    else:
      path_in = '.'
    if len(sys.argv) >= 4:
      path_out = sys.argv[3]
    else:
      path_out = '.'
    path_l = list()
    path_l.append(list())
    for T in [1,2,3,4]:
      path_l[-1].append(path_in+'/test_policy/ner_gibbs_T%d' % T)
    path_l.append(list())
    for thres in [0.5,1.0,1.5,2.0,2.5,3.0]:
      path_l[-1].append(path_in+'/test_policy/ner_entropy_%0.2f' % thres)
    path_l.append(list())
    for c in [0, 0.05, 0.1, 0.2, 0.3, 1]:
      path_l[-1].append(path_in+'/test_policy/ner_cyclic_value_%f' % c)
    policy_l = plot(path_l, ['Gibbs', 'Entropy', 'Policy'], path_out+'/ner.png')
    name_l = [[p.split('/')[-1] for p in path] for path in path_l]
    html = open(path_out+'/ner.html', 'w')
    html.write(PolicyResult.viscomp(list(itertools.chain(*policy_l)), \
                      list(itertools.chain(*name_l)), 'NER'))
  else:
    if len(sys.argv) >= 3:
      path_in = sys.argv[2]
    else:
      path_in = '.'
    if len(sys.argv) >= 4:
      path_out = sys.argv[3]
    else:
      path_out = '.'
    path_l = list()
    path_l.append(list())
    for T in [1,2,3,4]:
      path_l[-1].append(path_in+'/test_policy/%s_gibbs_T%d' % (name, T))
    path_l.append(list())
    for c in [0, 0.05, 0.1, 0.2, 0.3, 1]:
      path_l[-1].append(path_in+'/test_policy/%s_policy_c%f' % (name, c))
    """
    path_l.append(list())
    for thres in [0.5,1.0,1.5,2.0,2.5,3.0]:
      path_l[-1].append(path_in+'/test_policy/%s_entropy_%0.2f' % (name, thres))
    """
    policy_l = plot(path_l, ['Gibbs', 'Policy'], path_out+'/%s.png' % name)
    name_l = [[p.split('/')[-1] for p in path] for path in path_l]
    html = open(path_out+'/%s.html' % name, 'w')
    html.write(PolicyResult.viscomp(list(itertools.chain(*policy_l)), \
                      list(itertools.chain(*name_l)), 'POS'))
