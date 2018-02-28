#!/bin/bash
now=$(date +"%H_%M_%d_%m_%Y")
filename="results/result_$now.log"
echo "Время проведения теста: $(date)" | tee -a $filename
echo "Сохранено в файл $filename"

echo "Тестирование коэффициента использования канала (ИТП4-а)" | tee -a $filename
dd if=/dev/urandom of=data/test_rand.dat  bs=10M  count=2
(echo " in ping out -c 5 "; echo " iperf in out "; echo " in curl -o /dev/null http://10.0.0.2:8000/test_rand.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1.yaml single-host-udp.yaml h1 2>&1 | tee -a $filename
yes|rm data/test_rand.dat
echo "" | tee -a $filename

echo "Тестирование параллельных каналов передачи (ИТП4-б)" | tee -a $filename
echo "С использованием одного канала" | tee -a $filename
(echo " out iperf3 -s & "; sleep 2; echo " in iperf3 -c out "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1.yaml single-host-udp.yaml h1 2>&1 | tee -a $filename
echo "С использованием двух каналов" | tee -a $filename
(echo " out iperf3 -s & "; sleep 2; echo " in iperf3 -c out "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2.yaml single-host-udp.yaml h1 2>&1 | tee -a $filename

echo "Тестирование помехозащищенного кодирования (ИТП4-в)" | tee -a $filename
echo "Без применения кодов Рида-Соломона на канале с потерей пакетов 10%" | tee -a $filename
(echo " out iperf3 -s & "; sleep 5; echo " in iperf3 -c out -u -b3M "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1.yaml single-host-udp-loss.yaml h1 2>&1 | tee -a $filename
echo "С применением кодов Рида-Соломона на канале с потерей пакетов 10%" | tee -a $filename
(echo " out iperf3 -s & "; sleep 10; echo " in iperf3 -c out -u -b3M "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1_tiny.yaml single-host-udp-tiny-loss.yaml h1 2>&1 | tee -a $filename

echo "Тестирование уплотнения данных (ИТП4-г)" | tee -a $filename
dd if=/dev/urandom of=data/test_rand.dat  bs=10M  count=2
dd if=/dev/zero of=data/test_zero.dat  bs=10M  count=2
echo "Без использования zlib" | tee -a $filename
echo "Файл с повторяющейся информацией" | tee -a $filename
(echo " in curl -o /dev/null http://10.0.0.2:8000/test_zero.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2.yaml single-host-udp.yaml h1 2>&1 | tee -a $filename
echo "Файл со случайной информацией" | tee -a $filename
(echo " in curl -o /dev/null http://10.0.0.2:8000/test_rand.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2.yaml single-host-udp.yaml h1 2>&1 | tee -a $filename
echo "С использованием zlib" | tee -a $filename
echo "Файл с повторяющейся информацией" | tee -a $filename
(echo " in curl -o /dev/null http://10.0.0.2:8000/test_zero.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2_zlib.yaml single-host-udp.yaml h1 2>&1 | tee -a $filename
echo "Файл со случайной информацией" | tee -a $filename
(echo " in curl -o /dev/null http://10.0.0.2:8000/test_rand.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2_zlib.yaml single-host-udp.yaml h1 2>&1 | tee -a $filename
yes|rm data/test_rand.dat
yes|rm data/test_zero.dat
echo "" | tee -a $filename

echo "Тестирование дедупликации данных (ИТП4-д)" | tee -a $filename
dd if=/dev/urandom of=data/test_rand.dat  bs=10M  count=2
mkdir data_out
(echo " out rsync --daemon --config=/root/qnet/mininet-qnet-tap/rsyncd.conf "; sleep 2; echo " in rsync -vh 10.0.0.2::pickup/test_rand.dat /root/qnet/mininet-qnet-tap/data_out/test_rand.dat "; echo " in rsync -vh  10.0.0.2::pickup/test_rand.dat /root/qnet/mininet-qnet-tap/data_out/test_rand.dat "; echo " out dd if=/dev/urandom bs=1M count=1 >> /root/qnet/mininet-qnet-tap/data/test_rand.dat "; echo " in rsync -vh  10.0.0.2::pickup/test_rand.dat /root/qnet/mininet-qnet-tap/data_out/test_rand.dat "; echo " out truncate -s -1M /root/qnet/mininet-qnet-tap/data/test_rand.dat "; echo " in rsync -vh  10.0.0.2::pickup/test_rand.dat /root/qnet/mininet-qnet-tap/data_out/test_rand.dat "; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2.yaml single-host-udp.yaml h1 2>&1 | tee -a $filename
yes|rm -r data_out
yes|rm data/test_rand.dat
echo "" | tee -a $filename

echo "Тестирование кодирования данных (ИТП4-е)" | tee -a $filename
grep polkitd:x:997:995 data/test.dat | tee -a $filename
echo "Перехват пакетов без кодирования" | tee -a $filename
(echo " vhwrong tcpdump -i vhwrong-eth0 udp -w packets.pcap & "; sleep 1; echo " in curl -o /dev/null http://10.0.0.2:8000/test.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1_raw.yaml single-host-udp-wrong.yaml h1 2>&1 | tee -a $filename
echo "grep polkitd:x:997:995 packets.pcap" >> $filename
grep polkitd:x:997:995 packets.pcap | tee -a $filename
yes|rm packets.pcap
echo "Перехват пакетов с кодированием" | tee -a $filename
(echo " vhwrong tcpdump -i vhwrong-eth0 udp -w packets.pcap & "; sleep 1; echo " in curl -o /dev/null http://10.0.0.2:8000/test.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1.yaml single-host-udp-wrong.yaml h1 2>&1 | tee -a $filename
echo "grep polkitd:x:997:995 packets.pcap" >> $filename
grep polkitd:x:997:995 packets.pcap | tee -a $filename
yes|rm packets.pcap

echo "Сохранено в файл $filename"
