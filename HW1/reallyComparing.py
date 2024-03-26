
import json 
import pandas as pd
import sys


#get the name of the two files
file1 = sys.argv[1]
file2 = sys.argv[2]

#read the files
with open(file1) as f:
    data1 = json.load(f)
with open(file2) as f:
    data2 = json.load(f)
    
#for each big block in the json files compare the two files






