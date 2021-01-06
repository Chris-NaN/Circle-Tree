import random
# L1 = random.sample(range(1, 10), 5)
# print(L1)

# with open('out.txt') as f:


#!/usr/bin/python
# -*- coding: UTF-8 -*-

import sys, getopt

def main(argv):
   num_data = 0
   interval = 4
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
   print('输入的数据大小为：', num_data)
   print('输出的文件为：', outputfile)

   for i in range(0, num_data, interval):
      L1 = random.sample(range(0, num_data), interval)
      L1.sort()
      # print("range:(%d, %d)" %(i+1, i+int(num_data/5)))
      with open (outputfile, 'a+') as f:
         for d in L1:
            f.write(str(d)+'\n')

if __name__ == "__main__":
   main(sys.argv[1:])
