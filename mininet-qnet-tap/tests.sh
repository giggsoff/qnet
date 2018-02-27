#!/bin/bash
now=$(date +"%H_%M_%d_%m_%Y")
filename="./results/result_$now.log"
echo "Testing time $(date)" | tee $filename
echo "Saved to file $filename"
(sleep 10; echo " out iperf3 -s & "; sleep 2; echo " in iperf3 -c out "; sleep 1; echo " quit ") | python mininet-qnet-tap.py defaults_1.yaml single-host-udp.yaml h1 2>&1 | tee -a $filename
