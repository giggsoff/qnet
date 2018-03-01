#!/bin/bash
now=$(date +"%H_%M_%d_%m_%Y")
filename="results/result_$now.log"
filenamedocx="results/result_$now.docx"
echo "Тестирование ИТП4" | python makeword.py -s 2 -f $filenamedocx
echo "Время проведения теста: $(date)" | python makeword.py -c TextStyle -f $filenamedocx
echo "Сохранено в файл $filenamedocx"
echo "Тестирование коэффициента использования канала (ИТП4-а)"
echo "Тестирование коэффициента использования канала (ИТП4-а)" | python makeword.py -s 3 -f $filenamedocx
echo 'dd if=/dev/urandom of=data/test_rand.dat  bs=10M  count=2' | python makeword.py -c CodeStyle -f $filenamedocx
dd if=/dev/urandom of=data/test_rand.dat  bs=10M  count=2 | python makeword.py -c CodeStyle -f $filenamedocx
echo '(echo " in ping out -c 5 "; echo " iperf in out "; echo " in curl -o /dev/null http://10.0.0.2:8000/test_rand.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx' | python makeword.py -c CodeStyle -f $filenamedocx
(echo " in ping out -c 5 "; echo " iperf in out "; echo " in curl -o /dev/null http://10.0.0.2:8000/test_rand.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx
yes|rm data/test_rand.dat
echo "" | python makeword.py -c TextStyle -f $filenamedocx
echo "Тестирование параллельных каналов передачи (ИТП4-б)"
echo "Тестирование параллельных каналов передачи (ИТП4-б)" | python makeword.py -s 3 -f $filenamedocx
echo "С использованием одного канала" | python makeword.py -s 4 -f $filenamedocx
echo '(echo " out iperf3 -s & "; sleep 2; echo " in iperf3 -c out "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx' | python makeword.py -c CodeStyle -f $filenamedocx
(echo " out iperf3 -s & "; sleep 2; echo " in iperf3 -c out "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx
echo "С использованием двух каналов" | python makeword.py -s 4 -f $filenamedocx
echo '(echo " out iperf3 -s & "; sleep 2; echo " in iperf3 -c out "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx' | python makeword.py -c CodeStyle -f $filenamedocx
(echo " out iperf3 -s & "; sleep 2; echo " in iperf3 -c out "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx
echo "Тестирование помехозащищенного кодирования (ИТП4-в)"
echo "Тестирование помехозащищенного кодирования (ИТП4-в)" | python makeword.py -s 3 -f $filenamedocx
echo "Без применения кодов Рида-Соломона на канале с потерей пакетов 10%" | python makeword.py -s 4 -f $filenamedocx
echo '(echo " out iperf3 -s & "; sleep 5; echo " in iperf3 -c out -u -b3M "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1.yaml single-host-udp-loss.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx' | python makeword.py -c CodeStyle -f $filenamedocx
(echo " out iperf3 -s & "; sleep 5; echo " in iperf3 -c out -u -b3M "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1.yaml single-host-udp-loss.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx
echo "С применением кодов Рида-Соломона на канале с потерей пакетов 10%" | python makeword.py -s 4 -f $filenamedocx
echo '(echo " out iperf3 -s & "; sleep 10; echo " in iperf3 -c out -u -b3M "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1_tiny.yaml single-host-udp-tiny-loss.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx' | python makeword.py -c CodeStyle -f $filenamedocx
(echo " out iperf3 -s & "; sleep 10; echo " in iperf3 -c out -u -b3M "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1_tiny.yaml single-host-udp-tiny-loss.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx
echo "Тестирование уплотнения данных (ИТП4-г)"
echo "Тестирование уплотнения данных (ИТП4-г)" | python makeword.py -s 3 -f $filenamedocx
echo 'dd if=/dev/urandom of=data/test_rand.dat  bs=10M  count=2' | python makeword.py -c CodeStyle -f $filenamedocx
dd if=/dev/urandom of=data/test_rand.dat  bs=10M  count=2 | python makeword.py -c CodeStyle -f $filenamedocx
echo 'dd if=/dev/zero of=data/test_zero.dat  bs=10M  count=2' | python makeword.py -c CodeStyle -f $filenamedocx
dd if=/dev/zero of=data/test_zero.dat  bs=10M  count=2 | python makeword.py -c CodeStyle -f $filenamedocx
echo "Без использования zlib" | python makeword.py -s 4 -f $filenamedocx
echo "Файл с повторяющейся информацией" | python makeword.py -s 5 -f $filenamedocx
echo '(echo " in curl -o /dev/null http://10.0.0.2:8000/test_zero.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx' | python makeword.py -c CodeStyle -f $filenamedocx
(echo " in curl -o /dev/null http://10.0.0.2:8000/test_zero.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx
echo "Файл со случайной информацией" | python makeword.py -s 5 -f $filenamedocx
echo '(echo " in curl -o /dev/null http://10.0.0.2:8000/test_rand.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx' | python makeword.py -c CodeStyle -f $filenamedocx
(echo " in curl -o /dev/null http://10.0.0.2:8000/test_rand.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx
echo "С использованием zlib" | python makeword.py -s 4 -f $filenamedocx
echo "Файл с повторяющейся информацией" | python makeword.py -s 5 -f $filenamedocx
echo '(echo " in curl -o /dev/null http://10.0.0.2:8000/test_zero.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2_zlib.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx' | python makeword.py -c CodeStyle -f $filenamedocx
(echo " in curl -o /dev/null http://10.0.0.2:8000/test_zero.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2_zlib.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx
echo "Файл со случайной информацией" | python makeword.py -s 5 -f $filenamedocx
echo '(echo " in curl -o /dev/null http://10.0.0.2:8000/test_rand.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2_zlib.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx' | python makeword.py -c CodeStyle -f $filenamedocx
(echo " in curl -o /dev/null http://10.0.0.2:8000/test_rand.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2_zlib.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx
yes|rm data/test_rand.dat
yes|rm data/test_zero.dat
echo "" | python makeword.py -c TextStyle -f $filenamedocx
echo "Тестирование дедупликации данных (ИТП4-д)"
echo "Тестирование дедупликации данных (ИТП4-д)" | python makeword.py -s 3 -f $filenamedocx
echo 'dd if=/dev/urandom of=data/test_rand.dat  bs=10M  count=2' | python makeword.py -c CodeStyle -f $filenamedocx
dd if=/dev/urandom of=data/test_rand.dat  bs=10M  count=2 | python makeword.py -c CodeStyle -f $filenamedocx
echo 'mkdir data_out' | python makeword.py -c CodeStyle -f $filenamedocx
mkdir data_out
echo '(echo " out rsync --daemon --config=/root/qnet/mininet-qnet-tap/rsyncd.conf "; sleep 2; echo " in rsync -vh 10.0.0.2::pickup/test_rand.dat /root/qnet/mininet-qnet-tap/data_out/test_rand.dat "; echo " in rsync -vh  10.0.0.2::pickup/test_rand.dat /root/qnet/mininet-qnet-tap/data_out/test_rand.dat "; echo " out dd if=/dev/urandom bs=1M count=1 >> /root/qnet/mininet-qnet-tap/data/test_rand.dat "; echo " in rsync -vh  10.0.0.2::pickup/test_rand.dat /root/qnet/mininet-qnet-tap/data_out/test_rand.dat "; echo " out truncate -s -1M /root/qnet/mininet-qnet-tap/data/test_rand.dat "; echo " in rsync -vh  10.0.0.2::pickup/test_rand.dat /root/qnet/mininet-qnet-tap/data_out/test_rand.dat "; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx' | python makeword.py -c CodeStyle -f $filenamedocx
(echo " out rsync --daemon --config=/root/qnet/mininet-qnet-tap/rsyncd.conf "; sleep 2; echo " in rsync -vh 10.0.0.2::pickup/test_rand.dat /root/qnet/mininet-qnet-tap/data_out/test_rand.dat "; echo " in rsync -vh  10.0.0.2::pickup/test_rand.dat /root/qnet/mininet-qnet-tap/data_out/test_rand.dat "; echo " out dd if=/dev/urandom bs=1M count=1 >> /root/qnet/mininet-qnet-tap/data/test_rand.dat "; echo " in rsync -vh  10.0.0.2::pickup/test_rand.dat /root/qnet/mininet-qnet-tap/data_out/test_rand.dat "; echo " out truncate -s -1M /root/qnet/mininet-qnet-tap/data/test_rand.dat "; echo " in rsync -vh  10.0.0.2::pickup/test_rand.dat /root/qnet/mininet-qnet-tap/data_out/test_rand.dat "; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_2.yaml single-host-udp.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx
yes|rm -r data_out
yes|rm data/test_rand.dat
echo "" | python makeword.py -c TextStyle -f $filenamedocx
echo "Тестирование кодирования данных (ИТП4-е)"
echo "Тестирование кодирования данных (ИТП4-е)" | python makeword.py -s 3 -f $filenamedocx
echo "grep polkitd data/test.dat" | python makeword.py -c CodeStyle -f $filenamedocx
grep polkitd data/test.dat | python makeword.py -c CodeStyle -f $filenamedocx
echo "Перехват пакетов без кодирования" | python makeword.py -s 4 -f $filenamedocx
echo '(echo " vhwrong tcpdump -i vhwrong-eth0 udp -w packets.pcap & "; sleep 4; echo " in curl -o /dev/null http://10.0.0.2:8000/test.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1_raw.yaml single-host-udp-wrong.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx' | python makeword.py -c CodeStyle -f $filenamedocx
(echo " vhwrong tcpdump -i vhwrong-eth0 udp -w packets.pcap & "; sleep 4; echo " in curl -o /dev/null http://10.0.0.2:8000/test.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1_raw.yaml single-host-udp-wrong.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx
echo "grep polkitd packets.pcap" | python makeword.py -c CodeStyle -f $filenamedocx
grep polkitd packets.pcap | python makeword.py -c CodeStyle -f $filenamedocx
yes|rm packets.pcap
echo "Перехват пакетов с кодированием" | python makeword.py -s 4 -f $filenamedocx
echo '(echo " vhwrong tcpdump -i vhwrong-eth0 udp -w packets.pcap & "; sleep 4; echo " in curl -o /dev/null http://10.0.0.2:8000/test.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1.yaml single-host-udp-wrong.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx' | python makeword.py -c CodeStyle -f $filenamedocx
(echo " vhwrong tcpdump -i vhwrong-eth0 udp -w packets.pcap & "; sleep 4; echo " in curl -o /dev/null http://10.0.0.2:8000/test.dat "; sleep 1; echo " quit "; sleep 2) | stdbuf -o0 -e0 python mininet-qnet-tap.py defaults_1.yaml single-host-udp-wrong.yaml h1 2>&1 | python makeword.py -c CodeStyle -f $filenamedocx
echo "grep polkitd packets.pcap" | python makeword.py -c CodeStyle -f $filenamedocx
grep polkitd packets.pcap | python makeword.py -c CodeStyle -f $filenamedocx
yes|rm packets.pcap

echo "Сохранено в файл $filenamedocx"
