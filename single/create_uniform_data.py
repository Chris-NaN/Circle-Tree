import random
import numpy as np
# L1 = random.sample(range(1, 10), 5)
# print(L1)

# with open('out.txt') as f:


#!/usr/bin/python
# -*- coding: UTF-8 -*-

import sys, getopt

def main(argv):
   num_data = 0
   outputfile = ''
   try:
      opts, args = getopt.getopt(argv,"hn:o:",["ifile=","ofile="])
   except getopt.GetoptError:
      print('test.py -n <number of data> -o <outputfile>')
      sys.exit(2)
   for opt, arg in opts:
      if opt == '-h':
         print('test.py -n <number of data> -o <outputfile>')
         sys.exit()
      elif opt in ("-n"):
         num_data = int(arg)
      elif opt in ("-o", "--ofile"):
         outputfile = arg
      else:
         print('test.py -n <number of data> -o <outputfile>')
         sys.exit()
   print('输入的数据大小为：', num_data)
   print('输出的文件为：', outputfile)
   for i in range(0, num_data, int(num_data/5)):
      np.random.seed(0)
      L1 = np.random.normal(int(num_data/2), int(num_data/100), int(num_data/5))
      print("range:(%d, %d)" %(i+1, i+int(num_data/5)))
      with open (outputfile, 'a+') as f:
         for d in L1:
            f.write(str(int(d))+'\n')

if __name__ == "__main__":
   main(sys.argv[1:])
